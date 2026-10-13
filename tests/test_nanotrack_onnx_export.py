import unittest

import numpy as np
import torch
import torch.nn as nn

from scripts.export_nanotrack_onnx import compare_outputs, replace_hard_activations


class NanoTrackOnnxExportTest(unittest.TestCase):
    def test_hard_activation_replacement_is_numerically_equivalent(self):
        model = nn.Sequential(nn.Hardswish(), nn.Hardsigmoid())
        values = torch.linspace(-8.0, 8.0, 101)
        expected = model(values)

        replace_hard_activations(model)
        actual = model(values)

        self.assertNotIsInstance(model[0], nn.Hardswish)
        self.assertNotIsInstance(model[1], nn.Hardsigmoid)
        self.assertTrue(torch.allclose(expected, actual, atol=1e-7, rtol=0.0))

    def test_compare_outputs_reports_max_and_mean_absolute_error(self):
        reference = [np.asarray([1.0, 2.0], dtype=np.float32)]
        candidate = [np.asarray([1.0, 2.25], dtype=np.float32)]

        result = compare_outputs(reference, candidate)

        self.assertAlmostEqual(result[0]["max_abs_error"], 0.25)
        self.assertAlmostEqual(result[0]["mean_abs_error"], 0.125)
        self.assertEqual(result[0]["shape"], [2])

    def test_compare_outputs_rejects_shape_mismatch(self):
        with self.assertRaises(ValueError):
            compare_outputs(
                [np.zeros((1, 2), dtype=np.float32)],
                [np.zeros((2, 1), dtype=np.float32)],
            )


if __name__ == "__main__":
    unittest.main()
