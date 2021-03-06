#include "global.h"
void delete_object_example(const char* accesskey, const char* secretkey,
		const char* bucket, const char *region, const char * objectname,
		int ssl, int followlocation, int maxredirects) {
	SNDAECSHandler* handler = 0;
	SNDAECSResult* ret = 0;

	SNDAECSErrorCode retcode ;

	snda_ecs_global_init();
	handler = snda_ecs_init_handler();
	ret = snda_ecs_init_result();
    retcode = snda_ecs_delete_object(handler, accesskey,
			secretkey, bucket, objectname, region, ssl, followlocation,
			maxredirects, ret);
	if (retcode != SNDA_ECS_SUCCESS) {
		printf("ClientErrorMessage:%s", ret->error->handlererrmsg);
	} else if (ret->serverresponse->httpcode >= 300) {
		SNDAECSErrorResponseContent* content = snda_ecs_to_error_response(ret);
		if (content) {
			if (content->code) {
				printf("ErrorCode:%s\n", content->code);
			}
			if (content->message) {
				printf("ErrorMessage:%s\n", content->message);
			}
			if (content->resource) {
				printf("Resource:%s\n", content->resource);
			}
			if (content->requestid) {
				printf("RequestId:%s\n", content->requestid);
			}
			if (content->fullbody) {
				printf("AllErrorMessage:%s\n", content->fullbody);
			}
		}
		if(ret->serverresponse->httpcode == 505) {
		  printf("Please check your bucketname,accessKey,SecretAccessKey!\n");
		}
		snda_ecs_release_error_response_content(content);
	} else {
		printf("Delete Object success and the http code is:%d\n",
				ret->serverresponse->httpcode);
	}

	snda_ecs_release_handler(handler);
	snda_ecs_release_result(ret);
	snda_ecs_global_uninit();
}

int main() {
	char * accesskey = SNDA_ACCESSKEY;//;"your accessKey";
	char * secretkey = SNDA_ACCESS_SECRET;//"your secretKey";
	char * bucketname = SNDA_BUCKET_HUADONG;//"your bucketname";
	char * region = SDNA_REGION_HUADONG;//your region
	char * objectname = "test.sh";
	int followlocation = 0;
	int maxredirects = 0;
	int ssl = 0;
	delete_object_example(accesskey, secretkey, bucketname, region, objectname,
			ssl, followlocation, maxredirects);
	return 0;
}

