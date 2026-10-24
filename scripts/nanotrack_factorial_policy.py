"""新交叉对照实验的类别优先评分；历史 v1/v2 评分由原模块保留。"""

import math
from typing import Mapping


SCORING_VERSION = "nanotrack-category-first-v3"
AERIAL_DOMAINS = ("UAV123", "UAVDT", "VisDrone-MOT", "VisDrone-SOT")
EPSILON = 1e-12


def _finite(value: object, label: str) -> float:
    try:
        number = float(value)
    except (TypeError, ValueError, OverflowError) as error:
        raise ValueError("{} must be a finite number".format(label)) from error
    if not math.isfinite(number):
        raise ValueError("{} must be finite".format(label))
    return number


def _summary(metrics: Mapping[str, object]) -> dict:
    if not isinstance(metrics, Mapping):
        raise ValueError("metrics must be a mapping")
    domains = metrics.get("domains")
    if not isinstance(domains, Mapping):
        raise ValueError("metrics require domains")
    for name in AERIAL_DOMAINS:
        if name not in domains:
            raise ValueError("missing required aerial domain {}".format(name))

    categories = {"person": [], "vehicle": []}
    overalls = {}
    for name, values in domains.items():
        if not isinstance(values, Mapping):
            raise ValueError("domain {} metrics must be a mapping".format(name))
        overalls[name] = _finite(values.get("overall_auc"), "{}.overall_auc".format(name))
        for category, scores in categories.items():
            key = category + "_auc"
            if values.get(key) is not None:
                scores.append(_finite(values[key], "{}.{}".format(name, key)))

    result = {}
    for category, scores in categories.items():
        if not scores:
            raise ValueError("missing required {} macro metrics".format(category))
        result[category + "_macro"] = sum(scores) / len(scores)
    result["domain_macro"] = sum(overalls[name] for name in AERIAL_DOMAINS) / 4.0
    result["target_score_v3"] = _finite(
        0.4 * result["person_macro"]
        + 0.4 * result["vehicle_macro"]
        + 0.2 * result["domain_macro"],
        "target_score_v3",
    )
    result["domain_overall"] = overalls
    return result


def target_score_v3(metrics: Mapping[str, object]) -> float:
    """返回 0.4*P + 0.4*V + 0.2*D，忽略缓存旧评分与样本数量。"""
    return _summary(metrics)["target_score_v3"]


def guardrail_decision(candidate: Mapping[str, object], baseline: Mapping[str, object]) -> dict:
    """相对固定基线检查七个退化门；非法指标抛出 ValueError。"""
    current = _summary(candidate)
    reference = _summary(baseline)
    limits = {"got_val_ao": 0.01, "uav123_val_auc": 0.01,
              "person_macro": 0.015, "vehicle_macro": 0.015}
    for name in ("got_val_ao", "uav123_val_auc"):
        current[name] = _finite(candidate.get(name), "candidate.{}".format(name))
        reference[name] = _finite(baseline.get(name), "baseline.{}".format(name))
    for name in AERIAL_DOMAINS[1:]:
        current[name] = current["domain_overall"][name]
        reference[name] = reference["domain_overall"][name]
        limits[name] = 0.015

    checks = {}
    reasons = []
    for name, limit in limits.items():
        drop = reference[name] - current[name]
        passed = drop <= limit + EPSILON
        checks[name] = {"candidate": current[name], "baseline": reference[name],
                        "drop": drop, "max_drop": limit, "passed": passed}
        if not passed:
            reasons.append("{}_drop_over_{}".format(name, limit))
    return {
        "scoring_version": SCORING_VERSION,
        "qualified": not reasons,
        "target_score_v3": current["target_score_v3"],
        "baseline_target_score_v3": reference["target_score_v3"],
        "checks": checks,
        "reasons": reasons,
    }


def rank_key(metrics: Mapping[str, object], baseline: Mapping[str, object]) -> tuple:
    """供 max 或降序排序使用；调用者仍须过滤 qualified=False 的候选。

    全部候选不合格时不得选择 max 的返回对象。未过门候选使用负无穷，
    防止其 T_v3 参与合格候选排名。
    """
    decision = guardrail_decision(metrics, baseline)
    if not decision["qualified"]:
        return False, -math.inf
    return True, decision["target_score_v3"]


def final_promotion(candidate_by_seed: Mapping[object, Mapping[str, object]],
                    baseline: Mapping[str, object]) -> dict:
    """恰好三个种子全部过门，且等权平均 T_v3 比固定基线至少高 0.005。"""
    if not isinstance(candidate_by_seed, Mapping) or len(candidate_by_seed) != 3:
        raise ValueError("final promotion requires exactly three seeds")
    seed_decisions = {
        seed: guardrail_decision(metrics, baseline)
        for seed, metrics in candidate_by_seed.items()
    }
    baseline_score = target_score_v3(baseline)
    mean_score = sum(item["target_score_v3"] for item in seed_decisions.values()) / 3.0
    reasons = [
        "seed_{}:{}".format(seed, reason)
        for seed, decision in seed_decisions.items()
        for reason in decision["reasons"]
    ]
    if mean_score + EPSILON < baseline_score + 0.005:
        reasons.append("target_improvement_below_0.005")
    return {
        "scoring_version": SCORING_VERSION,
        "qualified": not reasons,
        "promoted": not reasons,
        "target_score_v3": mean_score,
        "baseline_target_score_v3": baseline_score,
        "target_improvement": mean_score - baseline_score,
        "seed_decisions": seed_decisions,
        "reasons": reasons,
    }
