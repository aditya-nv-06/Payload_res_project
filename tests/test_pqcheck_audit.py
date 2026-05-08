import subprocess
import sys
import tempfile
from pathlib import Path
import unittest


class TestPQCheckAudit(unittest.TestCase):
    def test_detects_unsafe_python_execute(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            bad = root / "bad.py"
            bad.write_text(
                "def run(cur, user):\n"
                "    cur.execute(f\"SELECT * FROM users WHERE name = '{user}'\")\n",
                encoding="utf-8",
            )

            proc = subprocess.run(
                [
                    sys.executable,
                    "tools/pqcheck_audit.py",
                    "--root",
                    str(root),
                ],
                cwd=Path(__file__).resolve().parent.parent,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertIn("Potential non-parameterized Python SQL execution", proc.stdout)

    def test_clean_file_has_no_high_finding(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            good = root / "good.py"
            good.write_text(
                "def run(cur, user_id):\n"
                "    cur.execute(\"SELECT * FROM users WHERE id = %s\", (user_id,))\n",
                encoding="utf-8",
            )

            proc = subprocess.run(
                [
                    sys.executable,
                    "tools/pqcheck_audit.py",
                    "--root",
                    str(root),
                ],
                cwd=Path(__file__).resolve().parent.parent,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertIn("No static findings detected.", proc.stdout)


if __name__ == "__main__":
    unittest.main()
