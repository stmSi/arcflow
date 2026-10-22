"""Protect the shared catalog contract and generated content."""
import copy
import importlib.util
from pathlib import Path
import tempfile
import sys
sys.dont_write_bytecode = True
import unittest

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('learning', ROOT / 'tools/build-learning.py')
learning = importlib.util.module_from_spec(spec)
spec.loader.exec_module(learning)


class LearningTests(unittest.TestCase):
    def setUp(self):
        self.catalog = learning.load_catalog(ROOT / 'docs/learning/catalog.json')

    def test_catalog_and_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            learning.generate(self.catalog, path / 'generated', path / 'site', 'test-build')
            c = (path / 'generated/onboarding_guides.inc').read_text(encoding="utf-8")
            web = (path / 'site/index.html').read_text(encoding="utf-8")
            printable = (path / 'site/print.html').read_text(encoding="utf-8")
            for guide in self.catalog['guides']:
                self.assertIn(guide['id'], c)
                self.assertIn(guide['slug'], web)
                self.assertIn(learning.html.escape(guide['title']), printable)
                for step in guide['steps']:
                    self.assertIn(learning.html.escape(step['text']), printable)
            self.assertIn('test-build', printable)

    def test_invalid_catalogs(self):
        mutations = [lambda c: c['guides'].append(c['guides'][0]),
                     lambda c: c['guides'][0].update(demo='typo'),
                     lambda c: c['guides'][0].update(steps=[]),
                     lambda c: c['guides'][0]['steps'][0].update(typo='text'),
                     lambda c: c['guides'][0].update(slug='../escape'),
                     lambda c: c['guides'][0].update(minutes=True)]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'catalog.json'
            for mutate in mutations:
                invalid = copy.deepcopy(self.catalog)
                mutate(invalid)
                path.write_text(learning.json.dumps(invalid))
                with self.assertRaises(ValueError):
                    learning.load_catalog(path)

    def test_escape_and_progress_revision(self):
        guide = self.catalog['guides'][0]
        previous = learning.fingerprint(guide)
        guide['steps'][0]['text'] = '</script><script>alert("test")</script>'
        self.assertNotEqual(previous, learning.fingerprint(guide))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            learning.generate(self.catalog, path / 'c', path / 'web', 'test')
            html = (path / 'web/index.html').read_text(encoding="utf-8")
            self.assertNotIn('</script><script>alert', html)
            self.assertIn('&lt;/script&gt;', html)


if __name__ == '__main__':
    unittest.main()
