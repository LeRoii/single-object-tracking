import copy
import json
import tempfile
import unittest
from pathlib import Path

import cv2
import numpy as np

from scripts.analyze_nanotrack_failures import (analyze_sequence, atomic_json, file_identity,
    failure_requests, make_contact_sheets, run_analysis, verify_inputs)
from scripts.build_nanotrack_failure_matched_pool import (build_pool, canonical_group,
    source_image_identity, verify_pool)


class FailureAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.image = self.root / '000001.jpg'
        cv2.imwrite(str(self.image), np.full((100, 200, 3), 128, np.uint8))

    def sequence(self, boxes=None):
        boxes = boxes or [[10, 10, 10, 10]] * 6
        return dict(name='uavdt__M0101__id1', source='UAVDT', source_video='M0101',
                    category='vehicle', boxes_xywh=boxes,
                    image_paths=[str(self.image)] * len(boxes), frame_ids=list(range(1, len(boxes)+1)))

    def test_zero_overlap_sustained_drift_without_semantic_guess(self):
        result = analyze_sequence(self.sequence(), [[10, 10, 10, 10]] + [[80, 50, 10, 10]] * 5)
        self.assertEqual(result['zero_iou_runs'], [[1, 5]])
        self.assertEqual(result['first_sustained_drift'], [1, 5])
        self.assertIn('sustained_drift', result['labels'])
        self.assertEqual(result['ambiguity']['label'], 'ambiguous_requires_human_review')
        for label in ('occlusion', 'distractor', 'identity_switch'):
            self.assertNotIn(label, result['labels'])
        self.assertEqual(result['frames'][1]['frame_id'], 2)

    def test_geometry_labels_have_measured_evidence(self):
        boxes = [[10, 10, 4, 4], [60, 10, 4, 4], [0, 0, 20, 20], [0, 0, 40, 40]]
        result = analyze_sequence(self.sequence(boxes), boxes)
        self.assertTrue({'tiny_target', 'rapid_motion', 'scale_change', 'boundary_truncation'} <= set(result['labels']))
        self.assertEqual(result['frames'][0]['image_size_wh'], [200, 100])
        self.assertEqual(result['frames'][1]['gt_normalized_displacement'], 12.5)
        self.assertEqual(result['frames'][2]['pred_gt_area_ratio'], 1)
        self.assertIsNone(result['ambiguity'])

    def test_training_requests_require_geometric_clue_at_low_overlap_frame(self):
        seq=self.sequence([[10,10,4,4]]*6)
        good=analyze_sequence(seq,seq['boxes_xywh'])
        self.assertEqual(failure_requests([good]),[])
        poor=analyze_sequence(seq,[[80,50,10,10]]*6)
        self.assertEqual(failure_requests([poor]),[dict(source='UAVDT',category='vehicle',reason='tiny_target')])

    def test_invalid_geometry_lengths_and_identity_fail_closed(self):
        for boxes in ([[1, 2, 0, 4]], [[1, 2, float('nan'), 4]], [[1, 2, -1, 4]]):
            with self.assertRaises(ValueError):
                analyze_sequence(self.sequence(boxes), boxes)
        seq = self.sequence()
        with self.assertRaises(ValueError):
            analyze_sequence(seq, [[1, 2, 3, 4]])
        seq['source_video'] = ''
        with self.assertRaises(ValueError):
            analyze_sequence(seq, seq['boxes_xywh'])

    def test_missing_image_and_duplicate_frame_identity_rejected(self):
        seq = self.sequence()
        seq['image_paths'][0] = str(self.root / 'absent.jpg')
        with self.assertRaises((ValueError, FileNotFoundError)):
            analyze_sequence(seq, seq['boxes_xywh'])

    def test_explicit_unscored_null_gt_breaks_drift_without_inventing_boxes(self):
        seq=self.sequence(); seq['boxes_xywh'][2]=None
        seq['scored_frame_mask']=[True,True,False,True,True,True]
        result=analyze_sequence(seq,[[80,50,10,10]]*6)
        self.assertEqual(result['zero_iou_runs'],[[0,1],[3,5]])
        self.assertIsNone(result['first_sustained_drift'])
        self.assertIsNone(result['frames'][2]['iou'])
        self.assertEqual(result['frames'][2]['geometry_labels'],[])
        seq['scored_frame_mask'][2]=True
        with self.assertRaises(ValueError): analyze_sequence(seq,[[80,50,10,10]]*6)
        seq = self.sequence()
        seq['frame_ids'][1] = 1
        with self.assertRaises(ValueError):
            analyze_sequence(seq, seq['boxes_xywh'])

    def test_semantics_need_explicit_annotation_provenance(self):
        annotation = self.root / 'labels.json'
        seq = self.sequence()
        atomic_json(annotation, {'event': 'occlusion', 'sequence': seq['name'], 'frame_ids': [2]})
        seq['explicit_events'] = [{'label': 'occlusion', 'frame_ids': [2],
                                   'annotation': file_identity(annotation)}]
        result = analyze_sequence(seq, [[80, 50, 10, 10]] * 6)
        self.assertIn('occlusion', result['labels'])
        seq['explicit_events'][0].pop('annotation')
        with self.assertRaises(ValueError):
            analyze_sequence(seq, [[80, 50, 10, 10]] * 6)

    def test_semantic_event_for_other_sequence_or_frames_rejected(self):
        annotation = self.root / 'labels.json'
        seq = self.sequence()
        atomic_json(annotation, {'event': 'occlusion', 'sequence': 'other', 'frame_ids': [2]})
        seq['explicit_events'] = [{'label':'occlusion', 'frame_ids':[2], 'annotation':file_identity(annotation)}]
        with self.assertRaises(ValueError):
            analyze_sequence(seq, [[80,50,10,10]]*6)

    def test_analysis_freezes_explicit_annotation_as_transitive_input(self):
        seq=self.sequence(); seq.update(window_start=1,window_end=6)
        annotation=self.root/'event.json'
        atomic_json(annotation,dict(event='occlusion',sequence=seq['name'],frame_ids=[2]))
        seq['explicit_events']=[dict(label='occlusion',frame_ids=[2],annotation=file_identity(annotation))]
        panel=self.root/'data/nanotrack_conservative_three_stage/val-aerial.json'
        atomic_json(panel,dict(split='validation',members=[seq['name']],sequences=[seq]))
        checkpoint=self.root/'model.pth'; atomic_json(checkpoint,{'model':'fixture'})
        config=self.root/'config.json'; atomic_json(config,{'config':'fixture'})
        model_identity=file_identity(checkpoint); config_identity=file_identity(config)
        base=self.root/'artifacts/nanotrack_got_replay_v2'
        candidate=dict(key='candidate',checkpoint=str(checkpoint),checkpoint_sha256=model_identity['sha256'],
                       metrics=dict(composite_score=.5),final_gate=dict(qualified=False))
        atomic_json(base/'run/candidate_table.json',dict(candidates=[candidate]))
        result=dict(snapshot=str(checkpoint),snapshot_sha256=model_identity['sha256'],
                    config=str(config),config_sha256=config_identity['sha256'],
                    panel_sha256=file_identity(panel)['sha256'],panel_split='validation',
                    sequences=[dict(**{k:seq[k] for k in ('name','source','source_video','category','window_start','window_end')},
                        frame_count=6,scored_frame_mask=[True]*6,predicted_xywh=seq['boxes_xywh'])])
        atomic_json(base/'baseline/baseline/aerial.json',result)
        atomic_json(base/'run/validation/candidate/aerial.json',result)
        out=self.root/'analysis-output'
        report=run_analysis(self.root,out,sheet_limit=0)
        self.assertIn(file_identity(annotation),report['inputs'])
        before=(out/'analysis.json').read_bytes()
        atomic_json(annotation,dict(event='distractor',sequence=seq['name'],frame_ids=[2]))
        with self.assertRaises(ValueError): run_analysis(self.root,out,sheet_limit=0)
        self.assertEqual(before,(out/'analysis.json').read_bytes())

    def test_frozen_input_drift_and_byte_stable_atomic_json(self):
        out = self.root / 'result.json'
        atomic_json(out, {'b': [2], 'a': 1})
        before = out.read_bytes()
        identity = file_identity(out)
        atomic_json(out, {'a': 1, 'b': [2]})
        self.assertEqual(before, out.read_bytes())
        verify_inputs([identity])
        atomic_json(out, {'a': 2})
        with self.assertRaises(ValueError):
            verify_inputs([identity])

    def test_contact_sheets_deterministic_capped_and_image_bound(self):
        records = []
        for i in range(27):
            seq = self.sequence(); seq['name'] += str(i)
            records.append(analyze_sequence(seq, [[80, 50, 10, 10]] * 6))
        sheets = make_contact_sheets(records, self.root / 'sheets', limit=24)
        self.assertEqual(len(sheets), 24)
        snapshot = copy.deepcopy(sheets)
        self.assertEqual(snapshot, make_contact_sheets(records, self.root / 'sheets', limit=24))
        self.assertEqual(sheets[0]['inputs'][0]['sha256'], file_identity(self.image)['sha256'])
        with self.assertRaises(ValueError):
            make_contact_sheets(records, self.root / 'sheets', limit=25)


class MatchedPoolTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.split = self.root / 'splits.json'
        atomic_json(self.split, {'sources': {'UAVDT': {'train': ['M0201'],
            'validation': ['M0101'], 'hidden-test': ['M0301']}}})
        self.panel = self.root / 'val.json'
        atomic_json(self.panel, {'split': 'validation', 'sequences': [dict(source='UAVDT', source_video='M0101')]})
        self.hidden=self.root/'hidden-membership.json'
        atomic_json(self.hidden,dict(split='hidden-test',members=['hidden1'],sequences=[
            dict(name='hidden1',source='UAVDT',source_video='M0301',
                 image_paths=['/must-not-open-hidden-images.jpg'],boxes_xywh='must-not-consume')]))
        self.audit = self.root / 'audit.json'
        self.train = self.root / 'train.json'
        self.image = self.root / 'UAVDT/UAV-benchmark-M/M0201/img000001.jpg'
        self.image.parent.mkdir(parents=True)
        cv2.imwrite(str(self.image), np.zeros((100,100,3), np.uint8))
        self.audit_data = {'source': 'UAVDT', 'train_videos': ['M0201'],
            'provenance': {'track1': {'source_video': 'M0201', 'category': 'vehicle', 'track_id': '00',
                'frames': {'1': {'bbox_xywh': [1, 2, 10, 10], 'image_path': str(self.image)}}}},
            'hard_reasons': {'track1': {'1': ['small', 'scale']}}}
        atomic_json(self.audit, self.audit_data)
        atomic_json(self.train, {'track1': {'00': {'000001': [1, 2, 11, 12]}}})

    def build(self):
        return build_pool([self.audit], [self.train], self.split, self.panel,
                          [{'source':'UAVDT','category':'vehicle','reason':'tiny_target'}], cap_per_request=3,
                          historical_hidden_panel=self.hidden)

    def test_training_membership_reason_mapping_and_read_only_verifier(self):
        pool = self.build()
        self.assertEqual(len(pool['members']), 1)
        member = pool['members'][0]
        self.assertEqual(member['matched_reason'], 'tiny_target')
        self.assertEqual(member['audit_reason'], 'small')
        out = self.root / 'pool.json'; atomic_json(out, pool)
        before = {str(p): (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.rglob('*') if p.is_file()}
        self.assertTrue(verify_pool(out)['verified'])
        after = {str(p): (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.rglob('*') if p.is_file()}
        self.assertEqual(before, after)

    def test_validation_alias_and_unknown_source_rejected(self):
        for video in ('C:\\frames\\m0101.MP4', 'uavdt__M0101__seg99', 'unknown'):
            d = copy.deepcopy(self.audit_data); d['provenance']['track1']['source_video'] = video
            atomic_json(self.audit, d)
            with self.assertRaises(ValueError):
                self.build()

    def test_hidden_group_excluded_without_opening_hidden_predictions(self):
        self.assertEqual(canonical_group('VisDrone-MOT', '/clips/UAV0000013_01392_V.MP4'),
                         canonical_group('VisDrone-SOT', 'uav0000013_00000_s'))
        atomic_json(self.split, {'sources': {'UAVDT': {'train': ['M0201'],
            'validation': ['M0101'], 'hidden-test': ['m0201.mp4']}}})
        atomic_json(self.hidden,dict(split='hidden-test',members=['hidden1'],sequences=[
            dict(name='hidden1',source='UAVDT',source_video='m0201.mp4')]))
        pool = self.build()
        self.assertEqual(pool['members'], [])
        self.assertEqual(pool['excluded_training_tracks'][0]['canonical_group'], 'uavdt:m0201')

    def test_tampered_member_and_annotation_hash_rejected(self):
        pool = self.build(); out = self.root / 'pool.json'
        pool['members'][0]['source_video'] = 'M0101'; atomic_json(out, pool)
        with self.assertRaises(ValueError): verify_pool(out)
        atomic_json(out, self.build())
        atomic_json(self.train, {})
        with self.assertRaises(ValueError): verify_pool(out)

    def test_only_requested_hard_reasons_match_and_stable_selection(self):
        a = self.build(); b = self.build()
        self.assertEqual(a, b)
        d = copy.deepcopy(self.audit_data); d['hard_reasons']['track1']['1'] = ['scale']
        atomic_json(self.audit, d)
        self.assertEqual(self.build()['members'], [])

    def test_diagnosis_provenance_verifies_and_rejects_hash_drift(self):
        diagnosis=self.root/'analysis.json'
        atomic_json(diagnosis, {'inputs':[], 'training_requests':[{'source':'UAVDT','category':'vehicle','reason':'tiny_target'}]})
        pool=self.build(); pool['diagnosis']=file_identity(diagnosis)
        out=self.root/'pool.json'; atomic_json(out,pool)
        self.assertTrue(verify_pool(out)['verified'])
        atomic_json(diagnosis, {'training_requests':[]})
        with self.assertRaises(ValueError): verify_pool(out)

    def test_declared_training_video_cannot_mask_validation_image_path(self):
        validation=self.root/'UAVDT/UAV-benchmark-M/M0101/img000001.jpg'
        validation.parent.mkdir(parents=True)
        cv2.imwrite(str(validation),np.zeros((100,100,3),np.uint8))
        d=copy.deepcopy(self.audit_data)
        d['provenance']['track1']['frames']['1']['image_path']=str(validation)
        atomic_json(self.audit,d)
        with self.assertRaises(ValueError): self.build()

    def test_declared_frame_cannot_mask_different_source_image_frame(self):
        other=self.image.with_name('img000002.jpg')
        cv2.imwrite(str(other),np.zeros((100,100,3),np.uint8))
        d=copy.deepcopy(self.audit_data)
        d['provenance']['track1']['frames']['1']['image_path']=str(other)
        atomic_json(self.audit,d)
        with self.assertRaises(ValueError): self.build()

    def test_pool_verifier_rejects_changed_diagnosis_dependency(self):
        dependency=self.root/'frozen-validation-predictions.json'
        atomic_json(dependency,dict(prediction='original'))
        diagnosis=self.root/'analysis.json'
        atomic_json(diagnosis,dict(inputs=[file_identity(dependency)],
            training_requests=[dict(source='UAVDT',category='vehicle',reason='tiny_target')]))
        pool=self.build(); pool['diagnosis']=file_identity(diagnosis)
        output=self.root/'pool.json';atomic_json(output,pool)
        self.assertTrue(verify_pool(output)['verified'])
        atomic_json(dependency,dict(prediction='changed but diagnosis JSON unchanged'))
        before=output.read_bytes()
        with self.assertRaises(ValueError):verify_pool(output)
        self.assertEqual(before,output.read_bytes())

    def test_hidden_panel_identity_and_subset_coverage_are_frozen(self):
        atomic_json(self.split, {'sources': {'UAVDT': {'train':['M0201'],
            'validation':['M0101'],'hidden-test':['M0301','M0401']}}})
        pool=self.build()
        self.assertIn(file_identity(self.hidden),pool['inputs'])
        self.assertEqual(pool['hidden_membership']['coverage']['UAVDT'],dict(
            panel_groups=['M0301'],split_groups=['M0301','M0401'],split_only_groups=['M0401']))

    def test_hidden_panel_group_outside_authoritative_split_rejected(self):
        atomic_json(self.hidden,dict(split='hidden-test',members=['hidden1'],sequences=[
            dict(name='hidden1',source='UAVDT',source_video='M9999')]))
        with self.assertRaises(ValueError):self.build()

    def test_hidden_panel_malformed_or_wrong_split_rejected(self):
        for payload in [dict(split='validation',members=['hidden1'],sequences=[dict(name='hidden1',source='UAVDT',source_video='M0301')]),
                        dict(split='hidden-test',members=['hidden1'],sequences=[dict(name='hidden1',source='UAVDT')]),
                        dict(split='hidden-test',members=['wrong'],sequences=[dict(name='hidden1',source='UAVDT',source_video='M0301')])]:
            atomic_json(self.hidden,payload)
            with self.assertRaises(ValueError):self.build()

    def test_hidden_membership_rejects_trailing_non_whitespace(self):
        valid=self.hidden.read_text()
        for suffix in ('{}','\n[]','trailing-junk'):
            self.hidden.write_text(valid+suffix)
            with self.assertRaises(ValueError):self.build()
        self.hidden.write_text(valid+'\n \t')
        self.assertEqual(len(self.build()['members']),1)

    def test_source_specific_layouts_and_symlink_resolution(self):
        layouts=[('UAV123','bike1','data_seq/UAV123_10fps/bike1/000001.jpg'),
                 ('UAVDT','M0201','UAVDT/UAV-benchmark-M/M0201/img000001.jpg'),
                 ('VisDrone-MOT','uav0000218_00001_v','VisDrone2019-MOT-train/sequences/uav0000218_00001_v/0000001.jpg'),
                 ('VisDrone-SOT','uav0000160_00000_s','VisDrone2019-SOT-train/sequences/uav0000160_00000_s/img0000001.jpg')]
        for source,video,relative in layouts:
            path=self.root/relative;path.parent.mkdir(parents=True,exist_ok=True)
            cv2.imwrite(str(path),np.zeros((10,10,3),np.uint8))
            self.assertEqual(source_image_identity(source,video,1,path,set())['source_video'],video)
            unknown=self.root/(source+'-unknown.jpg');cv2.imwrite(str(unknown),np.zeros((10,10,3),np.uint8))
            with self.assertRaises(ValueError):source_image_identity(source,video,1,unknown,set())
        val=self.root/'UAVDT/UAV-benchmark-M/M0101/img000001.jpg';val.parent.mkdir(parents=True)
        cv2.imwrite(str(val),np.zeros((10,10,3),np.uint8))
        link=self.root/'train-image-link.jpg';link.symlink_to(val)
        with self.assertRaises(ValueError):source_image_identity('UAVDT','M0201',1,link,{'uavdt:m0101'})


if __name__ == '__main__':
    unittest.main()
