/** @author joelai */

#include "aloe_unitest.h"

aloe_test_flag_t aloe_test_runner(aloe_test_case_t *suite_runner) {
	aloe_test_t *suite = aloe_container_of(suite_runner, aloe_test_t, runner);
	aloe_tailq_entry_t *qent;

	// The most use-case for setup on suite
	if ((suite_runner->flag_result == aloe_test_flag_result_pass) && suite->setup) {
		aloe_log_d("Setup test suite[%s]" aloe_endl, suite_runner->name);
		if ((suite->setup)(suite) != aloe_test_flag_result_pass) {
			suite_runner->flag_result = aloe_test_flag_result_failed_suite;
			if (!suite_runner->cause) suite_runner->cause = "SETUP";
			aloe_log_e("Setup failed test suite[%s]" aloe_endl, suite_runner->name);
		}
	} else {
		aloe_log_d("Start test suite[%s]" aloe_endl, suite_runner->name);
	}

	TAILQ_FOREACH(qent, &suite->cases, entry) {
		aloe_test_case_t *case_runner = aloe_container_of(qent,
				aloe_test_case_t, qent);

		// Populate prerequisite to contained suites and cases.
		if ((suite_runner->flag_result == aloe_test_flag_result_failed_suite) ||
				(suite_runner->flag_result == aloe_test_flag_result_prerequisite)) {
			case_runner->flag_result = aloe_test_flag_result_prerequisite;
			case_runner->cause = "PREREQUISITE";
			aloe_log_d("%s for test %s[%s]" aloe_endl,
					ALOE_TEST_RESULT_STR(case_runner->flag_result, "UNKNOWN result"),
					ALOE_TEST_CLASS_STR(case_runner->flag_class, "UNKNOWN class"),
					case_runner->name);

			if (case_runner->flag_class == aloe_test_flag_class_suite) {
				(case_runner->run)(case_runner);
			}
			continue;
		}

		// Test suite failure do not break containing suite.
		if (case_runner->flag_class == aloe_test_flag_class_suite) {
			case_runner->flag_result = (case_runner->run)(case_runner);
			if (case_runner->flag_result != aloe_test_flag_result_pass) {
				if (!case_runner->cause) case_runner->cause = "RUN";
				if (suite_runner->flag_result == aloe_test_flag_result_pass) {
					suite_runner->flag_result = aloe_test_flag_result_failed;
					suite_runner->cause = case_runner->name;
				}
			}
			continue;
		}

		aloe_log_d("Start test case[%s]" aloe_endl, case_runner->name);
		if ((case_runner->flag_result = (case_runner->run)(case_runner)) !=
				aloe_test_flag_result_pass) {
			if (!case_runner->cause) case_runner->cause = "RUN";
			aloe_log_d("%s for test case[%s]" aloe_endl
					"  Cause: %s" aloe_endl,
					ALOE_TEST_RESULT_STR(case_runner->flag_result, "UNKNOWN result"),
					case_runner->name, case_runner->cause);
			if (suite_runner->flag_result < case_runner->flag_result) {
				suite_runner->flag_result = case_runner->flag_result;
				suite_runner->cause = case_runner->name;
				aloe_log_d("%s for test suite[%s]" aloe_endl
						"  Cause: %s" aloe_endl,
						ALOE_TEST_RESULT_STR(suite_runner->flag_result, "UNKNOWN result"),
						suite_runner->name, suite_runner->cause);
			}
		}
		aloe_log_d("Stopped test case[%s]" aloe_endl, case_runner->name);
	}

	if (suite->shutdown) {
		(suite->shutdown)(suite);
		aloe_log_d("Shutdown test suite[%s]" aloe_endl, suite_runner->name);
	} else {
		aloe_log_d("Stopped test suite[%s]" aloe_endl, suite_runner->name);
	}

	return suite_runner->flag_result;
}

int aloe_test_report(aloe_test_t *suite, aloe_test_report_t *report_runner) {
#define report_log(_lvl, _args...) if (report_runner->log) { \
		(*report_runner->log)((unsigned)(_lvl), __func__, __LINE__, _args); \
}
#define report_log_d(_args...) report_log(aloe_log_level_debug, _args)
#define report_log_e(_args...) report_log(aloe_log_level_error, _args)

	aloe_tailq_entry_t *qent;
	int r = 0, pass = 0, failed = 0, total = 0, failed_prereq = 0;

	TAILQ_FOREACH(qent, &suite->cases, entry) {
		aloe_test_case_t *case_runner = aloe_container_of(qent,
				aloe_test_case_t, qent);

		if (report_runner && report_runner->runner &&
				((r = (*report_runner->runner)(case_runner, report_runner)) != 0)) {
			report_log_e("Report runner break" aloe_endl);
			break;
		}

		if (case_runner->flag_class == aloe_test_flag_class_suite) {
			if ((r = aloe_test_report(aloe_container_of(case_runner,
					aloe_test_t, runner), report_runner)) != 0) {
				report_log_e("Report suite break" aloe_endl);
				break;
			}
			continue;
		}
		total++;
		switch(case_runner->flag_result) {
		case aloe_test_flag_result_pass:
			pass++;
			break;
		case aloe_test_flag_result_failed:
			failed++;
			break;
		case aloe_test_flag_result_failed_suite:
			failed++;
			break;
		case aloe_test_flag_result_prerequisite:
			failed++;
			failed_prereq++;
			break;
		default:
			failed++;
			break;
		}
		if (case_runner->flag_result == aloe_test_flag_result_failed ||
				case_runner->flag_result == aloe_test_flag_result_failed_suite) {
			report_log_d("Report result %s, test case[%s], #%d in suite[%s]" aloe_endl
					"  Cause: %s" aloe_endl,
					ALOE_TEST_RESULT_STR(case_runner->flag_result, "UNKNOWN"),
					case_runner->name, total, suite->runner.name,
					(case_runner->cause ? case_runner->cause : "UNKNOWN"));
		} else {
			report_log_d("Report result %s, test case[%s], #%d in suite[%s]" aloe_endl,
					ALOE_TEST_RESULT_STR(case_runner->flag_result, "UNKNOWN"),
					case_runner->name, total, suite->runner.name);
		}
	}

	report_log_d("%s result %s, test suite[%s]" aloe_endl
			"  Summary test cases PASS: %d, FAILED: %d(PREREQUISITE: %d), TOTAL: %d" aloe_endl,
			(r != 0 ? "Report(incomplete)" : "Report"),
			ALOE_TEST_RESULT_STR(suite->runner.flag_result, "UNKNOWN"),
			suite->runner.name, pass, failed, failed_prereq, total);

	if (report_runner) {
		report_runner->total += total;
		report_runner->pass += pass;
		report_runner->failed += failed;
		report_runner->failed_prereq += failed_prereq;
	}
	return r;
}
