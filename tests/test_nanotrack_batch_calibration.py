import unittest
from pathlib import Path

from scripts.calibrate_nanotrack_got_replay_batch import cuda_free_mib, resolve_cli_path


class FakeCudaWithNativeMemory:
    @staticmethod
    def mem_get_info():
        return 3 * 1024 ** 2, 6 * 1024 ** 2


class FakeCudaWithoutNativeMemory:
    pass


class BatchCalibrationCompatibilityTest(unittest.TestCase):
    def test_resolves_relative_output_from_invocation_directory(self):
        invocation = Path("/tmp/nanotrack-invocation")
        self.assertEqual(
            resolve_cli_path(Path("artifacts/calibration.json"), invocation),
            invocation / "artifacts/calibration.json",
        )

    def test_uses_native_mem_get_info_when_available(self):
        self.assertEqual(cuda_free_mib(FakeCudaWithNativeMemory(),
                                      lambda command: self.fail(str(command))), 3.0)

    def test_falls_back_to_nvidia_smi_on_torch_1_7(self):
        seen = []

        def runner(command):
            seen.append(command)
            return b"4876\n"

        self.assertEqual(cuda_free_mib(FakeCudaWithoutNativeMemory(), runner), 4876.0)
        self.assertEqual(seen, [[
            "nvidia-smi", "--query-gpu=memory.free",
            "--format=csv,noheader,nounits", "--id=0",
        ]])

    def test_rejects_invalid_nvidia_smi_memory_output(self):
        with self.assertRaises(RuntimeError):
            cuda_free_mib(FakeCudaWithoutNativeMemory(), lambda command: b"N/A\n")


if __name__ == "__main__":
    unittest.main()
