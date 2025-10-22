// Tests de job manager
#include "test_utils.h"
#include "../src/core/job_manager.h"
#include <stdlib.h>
#include <string.h>

TEST(test_job_submit_and_status) {
	job_manager_init("data/jobs");

	char *job_id = job_submit("isprime", "{\"n\":97}", 0);
	ASSERT_NOT_NULL(job_id);

	job_status_info_t info;
	int r = job_get_status(job_id, &info);
	ASSERT_EQ(r, 0);
	ASSERT_EQ(info.status, JOB_STATUS_QUEUED);

	free(job_id);
}

TEST(test_job_mark_running_and_done) {
	char *job_id = job_submit("dummy", NULL, 0);
	ASSERT_NOT_NULL(job_id);

	int r = job_mark_running(job_id);
	ASSERT_EQ(r, 0);

	job_status_info_t info;
	job_get_status(job_id, &info);
	ASSERT_EQ(info.status, JOB_STATUS_RUNNING);

	r = job_mark_done(job_id, "{\"result\":123}");
	ASSERT_EQ(r, 0);

	char *res = job_get_result(job_id);
	ASSERT_NOT_NULL(res);
	ASSERT_TRUE(strstr(res, "result") != NULL);
	free(res);

	free(job_id);
}

TEST(test_job_mark_error_and_get_result) {
	char *job_id = job_submit("errtask", NULL, 0);
	ASSERT_NOT_NULL(job_id);

	int r = job_mark_running(job_id);
	ASSERT_EQ(r, 0);

	r = job_mark_error(job_id, "something went wrong");
	ASSERT_EQ(r, 0);

	char *res = job_get_result(job_id);
	ASSERT_NOT_NULL(res);
	ASSERT_TRUE(strstr(res, "error") != NULL);
	free(res);

	free(job_id);
}

TEST(test_job_cancel) {
	char *job_id = job_submit("cancelme", NULL, 0);
	ASSERT_NOT_NULL(job_id);

	int r = job_cancel(job_id);
	ASSERT_EQ(r, 0);

	job_status_info_t info;
	job_get_status(job_id, &info);
	ASSERT_EQ(info.status, JOB_STATUS_CANCELED);

	free(job_id);
}
