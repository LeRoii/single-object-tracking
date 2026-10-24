import copy
import unittest

from scripts.run_nanotrack_got_replay import (
    build_metrics,
    category_macro,
    composite_score,
    target_score,
)

try:
    from scripts.nanotrack_factorial_policy import (
        SCORING_VERSION,
        final_promotion,
        guardrail_decision,
        rank_key,
        target_score_v3,
    )
except ImportError as error:
    _IMPORT_ERROR = error
else:
    _IMPORT_ERROR = None


DOMAINS = ("UAV123", "UAVDT", "VisDrone-MOT", "VisDrone-SOT")


def metrics(score=0.5):
    """Use the real evaluator adapter, without loading data or models."""
    return build_metrics(
        {"metrics": {"sources": {
            name: {"person_auc": score, "vehicle_auc": score,
                   "overall_auc": score}
            for name in DOMAINS[1:]
        }}},
        {"overall": {"success_auc": score}, "categories": {
            "person": {"success_auc": score},
            "vehicle": {"success_auc": score},
        }},
        {"metrics": {"ao": 0.6, "sr_0_5": 0.55, "sr_0_75": 0.35}},
    )


def sparse_metrics():
    result = metrics()
    result["domains"] = {
        "UAV123": {"person_auc": 0.2, "vehicle_auc": 0.3, "overall_auc": 0.4},
        "UAVDT": {"person_auc": None, "vehicle_auc": 0.6, "overall_auc": 0.5},
        "VisDrone-MOT": {"person_auc": 0.8, "vehicle_auc": 0.9, "overall_auc": 0.6},
        "VisDrone-SOT": {"overall_auc": 0.7},
    }
    result["uav123_val_auc"] = 0.4
    return result


class FactorialPolicyTest(unittest.TestCase):
    def setUp(self):
        if _IMPORT_ERROR is not None:
            self.fail("v3 policy API is missing: {}".format(_IMPORT_ERROR))

    def test_person_macro_has_weight_point_four(self):
        candidate = metrics()
        for domain in candidate["domains"].values():
            domain["person_auc"] = 0.6
        self.assertAlmostEqual(target_score_v3(candidate), 0.54)

    def test_vehicle_macro_has_weight_point_four(self):
        candidate = metrics()
        for domain in candidate["domains"].values():
            domain["vehicle_auc"] = 0.6
        self.assertAlmostEqual(target_score_v3(candidate), 0.54)

    def test_overall_term_is_equal_four_domain_macro(self):
        candidate = metrics()
        for name, value in zip(DOMAINS, (0.1, 0.3, 0.5, 0.7)):
            candidate["domains"][name]["overall_auc"] = value
        self.assertAlmostEqual(target_score_v3(candidate), 0.48)

    def test_missing_categories_average_only_present_domains(self):
        # P=0.5, V=0.6, D=0.55, so T_v3=0.55.
        self.assertAlmostEqual(target_score_v3(sparse_metrics()), 0.55)

    def test_frame_trajectory_and_manifest_counts_do_not_weight_score(self):
        candidate = sparse_metrics()
        for name, count in zip(DOMAINS, (1, 100, 10000, 1000000)):
            candidate["domains"][name].update(
                frame_count=count, trajectory_count=count, manifest_frequency=count
            )
        self.assertAlmostEqual(target_score_v3(candidate), 0.55)

    def test_cached_historical_scores_and_macros_are_not_consumed(self):
        candidate = metrics()
        candidate.update(target_score=0.99, composite_score=0.99,
                         person_macro=0.99, vehicle_macro=0.99)
        self.assertAlmostEqual(target_score_v3(candidate), 0.5)

    def test_ranking_uses_target_when_got_and_uav_prefer_other_candidate(self):
        baseline = metrics()
        better_target = metrics(0.506)
        better_target.update(got_val_ao=0.591, uav123_val_auc=0.491)
        better_generalization = metrics(0.505)
        better_generalization.update(got_val_ao=0.8, uav123_val_auc=0.8)
        self.assertEqual(rank_key(better_target, baseline), (True, 0.506))
        self.assertIs(max((better_target, better_generalization),
                          key=lambda item: rank_key(item, baseline)), better_target)

    def test_ranking_filters_guardrail_failure_even_with_higher_target(self):
        baseline = metrics()
        failed = metrics(0.9)
        failed["got_val_ao"] = 0.589
        passed = metrics(0.505)
        self.assertEqual(rank_key(failed, baseline), (False, -float("inf")))
        self.assertIs(max((failed, passed), key=lambda item: rank_key(item, baseline)),
                      passed)

    def test_all_guardrail_boundaries_are_inclusive(self):
        baseline = metrics()
        for name in ("got_val_ao", "uav123_val_auc", "person_macro", "vehicle_macro",
                     "UAVDT", "VisDrone-MOT", "VisDrone-SOT"):
            with self.subTest(guardrail=name):
                candidate = metrics()
                self._set_drop(candidate, name, 0.01 if "val_" in name else 0.015)
                decision = guardrail_decision(candidate, baseline)
                self.assertTrue(decision["qualified"], decision)
                self.assertEqual(decision["reasons"], [])

    def test_each_guardrail_rejects_drop_beyond_boundary(self):
        baseline = metrics()
        for name in ("got_val_ao", "uav123_val_auc", "person_macro", "vehicle_macro",
                     "UAVDT", "VisDrone-MOT", "VisDrone-SOT"):
            with self.subTest(guardrail=name):
                candidate = metrics()
                self._set_drop(candidate, name, 0.010001 if "val_" in name else 0.015001)
                decision = guardrail_decision(candidate, baseline)
                self.assertFalse(decision["qualified"])
                self.assertTrue(any(name in reason for reason in decision["reasons"]))

    def _set_drop(self, candidate, name, drop):
        if name in ("got_val_ao", "uav123_val_auc"):
            candidate[name] -= drop
        elif name.endswith("_macro"):
            key = name.replace("_macro", "_auc")
            for domain in candidate["domains"].values():
                domain[key] -= drop
        else:
            candidate["domains"][name]["overall_auc"] -= drop

    def test_uav123_uses_stricter_single_point_zero_one_gate(self):
        candidate = metrics()
        candidate["uav123_val_auc"] = 0.489
        candidate["domains"]["UAV123"]["overall_auc"] = 0.489
        decision = guardrail_decision(candidate, metrics())
        self.assertFalse(decision["qualified"])
        self.assertEqual(len(decision["reasons"]), 1)
        self.assertIn("uav123_val_auc", decision["reasons"][0])

    def test_guardrails_are_all_reported_and_versioned(self):
        candidate = metrics(0.48)
        candidate["got_val_ao"] = 0.58
        decision = guardrail_decision(candidate, metrics())
        self.assertEqual(len(decision["reasons"]), 7)
        self.assertEqual(decision["scoring_version"], SCORING_VERSION)
        self.assertEqual(len(decision["checks"]), 7)

    def test_final_promotion_uses_three_seed_arithmetic_mean_at_boundary(self):
        candidates = {11: metrics(0.5), 22: metrics(0.5), 33: metrics(0.515)}
        decision = final_promotion(candidates, metrics())
        self.assertTrue(decision["promoted"], decision)
        self.assertTrue(decision["qualified"])
        self.assertAlmostEqual(decision["target_score_v3"], 0.505)
        self.assertAlmostEqual(decision["target_improvement"], 0.005)
        self.assertEqual(decision["reasons"], [])
        self.assertEqual(len(decision["seed_decisions"]), 3)
        self.assertEqual(decision["scoring_version"], SCORING_VERSION)

    def test_final_promotion_rejects_mean_gain_below_threshold(self):
        candidates = {11: metrics(0.5), 22: metrics(0.5), 33: metrics(0.514997)}
        decision = final_promotion(candidates, metrics())
        self.assertFalse(decision["promoted"])
        self.assertIn("target_improvement_below_0.005", decision["reasons"])

    def test_final_promotion_requires_every_seed_guardrail(self):
        candidates = {11: metrics(0.52), 22: metrics(0.52), 33: metrics(0.52)}
        candidates[22]["got_val_ao"] = 0.589
        decision = final_promotion(candidates, metrics())
        self.assertFalse(decision["promoted"])
        self.assertFalse(decision["seed_decisions"][22]["qualified"])
        self.assertTrue(any("22" in reason for reason in decision["reasons"]))

    def test_final_promotion_rejects_illegal_seed_counts_and_nonmapping(self):
        for candidates in ({}, {11: metrics()}, {11: metrics(), 22: metrics()},
                           {seed: metrics() for seed in range(4)},
                           [metrics(), metrics(), metrics()]):
            with self.subTest(candidates=candidates):
                with self.assertRaisesRegex(ValueError, "three|3"):
                    final_promotion(candidates, metrics())

    def test_score_rejects_each_missing_required_domain(self):
        for name in DOMAINS:
            candidate = metrics()
            del candidate["domains"][name]
            with self.subTest(domain=name), self.assertRaisesRegex(ValueError, name):
                target_score_v3(candidate)

    def test_score_rejects_absent_person_or_vehicle_macro(self):
        for category in ("person", "vehicle"):
            candidate = metrics()
            for domain in candidate["domains"].values():
                domain[category + "_auc"] = None
            with self.subTest(category=category), self.assertRaisesRegex(ValueError, category):
                target_score_v3(candidate)

    def test_score_rejects_nonfinite_domain_metrics(self):
        for value in (float("nan"), float("inf"), -float("inf")):
            for name in DOMAINS:
                for term in ("person_auc", "vehicle_auc", "overall_auc"):
                    candidate = metrics()
                    candidate["domains"][name][term] = value
                    with self.subTest(value=value, domain=name, term=term):
                        with self.assertRaisesRegex(ValueError, "finite"):
                            target_score_v3(candidate)

    def test_score_rejects_malformed_required_metrics(self):
        invalid = [None, {}, {"domains": []}, {"domains": {}}]
        for value in (None, "invalid", {}):
            candidate = metrics()
            candidate["domains"]["UAVDT"] = value
            invalid.append(candidate)
        for value in (None, "invalid"):
            candidate = metrics()
            candidate["domains"]["UAVDT"]["overall_auc"] = value
            invalid.append(candidate)
        for candidate in invalid:
            with self.subTest(candidate=candidate), self.assertRaises(ValueError):
                target_score_v3(candidate)

    def test_guardrail_rejects_missing_or_nonfinite_generalization_metrics(self):
        for key in ("got_val_ao", "uav123_val_auc"):
            for value in (None, "invalid", float("nan"), float("inf"), -float("inf")):
                for invalid_baseline in (False, True):
                    candidate, baseline = metrics(), metrics()
                    (baseline if invalid_baseline else candidate)[key] = value
                    with self.subTest(key=key, value=value, baseline=invalid_baseline):
                        with self.assertRaisesRegex(ValueError, key):
                            guardrail_decision(candidate, baseline)
            candidate = metrics()
            del candidate[key]
            with self.assertRaisesRegex(ValueError, key):
                guardrail_decision(candidate, metrics())

    def test_final_and_rank_cannot_bypass_invalid_metrics(self):
        candidate = metrics()
        candidate["domains"]["UAVDT"]["person_auc"] = float("nan")
        with self.assertRaises(ValueError):
            rank_key(candidate, metrics())
        with self.assertRaises(ValueError):
            final_promotion({11: metrics(), 22: candidate, 33: metrics()}, metrics())
        baseline = copy.deepcopy(candidate)
        with self.assertRaises(ValueError):
            final_promotion({11: metrics(), 22: metrics(), 33: metrics()}, baseline)


class HistoricalCompatibilityTest(unittest.TestCase):
    def test_old_domain_first_target_and_category_macros_are_unchanged(self):
        candidate = sparse_metrics()
        self.assertAlmostEqual(target_score(candidate), 0.5866666666666667)
        self.assertAlmostEqual(category_macro(candidate, "person"), 0.5)
        self.assertAlmostEqual(category_macro(candidate, "vehicle"), 0.6)

    def test_old_composite_retains_seventy_fifteen_fifteen_weights(self):
        # Historical T=0.586666..., GOT=0.6, UAV=0.4 gives C=0.560666...
        self.assertAlmostEqual(composite_score(sparse_metrics()), 0.5606666666666666)


if __name__ == "__main__":
    unittest.main()
