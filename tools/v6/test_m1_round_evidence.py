import copy
import unittest

from tools.v6.m1_round_evidence import FAULT_CHECKS, SCHEMA, summarize


def valid_input():
    identity = {'app_sha256': 'a' * 64, 'elf_sha256': 'b' * 64}
    server = {'repo': 'org/server', 'commit': 'c' * 40,
              'image': 'registry/server@sha256:' + 'd' * 64}
    rounds = []
    for n in range(1, 21):
        rounds.append({'round': n, 'scenario': ('short', 'long', 'silence', 'interruption')[(n - 1) % 4],
                       'session_id': 'session-001', 'candidate': dict(identity), 'server': dict(server),
                       'device_restarted': False, 'service_restarted': False, 'manual_recovery': False,
                       'latencies_ms': {'asr_final': {'value': n, 'clock': 'service_monotonic'},
                                        'llm_first_token': {'value': n + 1, 'clock': 'service_monotonic'},
                                        'tts_first_audio': {'value': n + 2, 'clock': 'service_monotonic'},
                                        'audible_first_response': {'value': n + 3, 'clock': 'device_monotonic'}},
                       'resources': {'heap_min_bytes': 1000 - n, 'psram_min_bytes': None},
                       'flags': {key: False for key in ('misrecapture', 'stuck', 'crash', 'wdt', 'reboot',
                                                               'looping_capture', 'lost_response')}})
    return {'schema': SCHEMA, 'session_id': 'session-001', 'candidate': identity, 'server': server,
            'config_version': 'config-v1', 'provider_versions': {'llm': 'l1', 'asr': 'a1', 'tts': 't1'},
            'rounds': rounds, 'fault_checks': {key: 'RECORDED' for key in FAULT_CHECKS}}


class M1RoundEvidenceTests(unittest.TestCase):
    def test_valid_run_reports_aggregates_and_keeps_clock_domains_separate(self):
        result = summarize(valid_input())
        self.assertEqual(result['status'], 'NOT_VERIFIED')  # PSRAM was explicitly N/A.
        self.assertEqual(result['round_count'], 20)
        self.assertEqual(result['scenario_coverage'], {'short': 5, 'long': 5, 'silence': 5, 'interruption': 5})
        self.assertEqual(result['metrics']['asr_final']['clock_domains'], ['service_monotonic'])
        self.assertEqual(result['metrics']['audible_first_response']['clock_domains'], ['device_monotonic'])
        self.assertEqual(result['metrics']['asr_final']['max_ms'], 20.0)

    def test_complete_metrics_and_fault_records_can_pass_structure_only(self):
        doc = valid_input()
        for row in doc['rounds']:
            row['resources']['psram_min_bytes'] = 500
        self.assertEqual(summarize(doc)['status'], 'PASS')

    def test_rejects_missing_identity_wrong_round_order_or_recovery(self):
        cases = []
        d = valid_input(); d['candidate']['app_sha256'] = 'a' * 9; cases.append(d)
        d = valid_input(); d['rounds'][1]['round'] = 1; cases.append(d)
        d = valid_input(); d['rounds'][4]['manual_recovery'] = True; cases.append(d)
        for doc in cases:
            with self.subTest(doc=doc):
                with self.assertRaises(ValueError): summarize(doc)

    def test_rejects_nonfinite_and_negative_metrics_and_mixed_session(self):
        for value in (-1, float('nan'), float('inf')):
            doc = valid_input(); doc['rounds'][0]['latencies_ms']['asr_final']['value'] = value
            with self.subTest(value=value), self.assertRaises(ValueError): summarize(doc)
        doc = valid_input(); doc['rounds'][0]['session_id'] = 'other-session'
        with self.assertRaises(ValueError): summarize(doc)

    def test_missing_fault_checks_and_explicit_na_never_become_pass(self):
        doc = valid_input(); doc['fault_checks'].pop('wake_without_asr')
        result = summarize(doc)
        self.assertEqual(result['status'], 'NOT_VERIFIED')
        self.assertTrue(any('wake_without_asr' in reason for reason in result['reasons']))
        doc = valid_input(); doc['rounds'][0]['latencies_ms']['tts_first_audio']['value'] = None
        self.assertEqual(summarize(doc)['metrics']['tts_first_audio']['status'], 'NOT_VERIFIED')

    def test_critical_failure_is_fail_and_outputs_no_prompt_fields(self):
        doc = valid_input(); doc['rounds'][0]['flags']['wdt'] = True
        doc['prompt'] = 'private text'; doc['rounds'][0]['transcript'] = 'private transcript'
        result = summarize(doc)
        self.assertEqual(result['status'], 'FAIL')
        self.assertNotIn('prompt', result)
        self.assertNotIn('transcript', result)


if __name__ == '__main__':
    unittest.main()
