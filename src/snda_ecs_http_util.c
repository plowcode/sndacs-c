/*
 * ProvideBy: SNDA Cloud Computing(http://www.grandcloud.cn).
 *
 * Header file of c sdk for Elastic Cloud Storage(ECS) of GrandCloud.
 *
 * This header file defines http utl for ECS sdk api.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "snda_ecs_http_util.h"
#include "snda_ecs_constants.h"
#include "snda_ecs_encode.h"
#include "snda_ecs_sdk.h"
#include "snda_ecs_common_util.h"

char* create_url(char* urlbuff,
		const char* region,
		const char* resource,
		int usessl)
{
	char* ptr = urlbuff;
	int len = 0;
	char* prefix = "http://";
	int port = 80;
	if (usessl) {
		prefix = "https://";
		port = 443;
	}

	const char* host = snda_ecs_default_hostname;
	if (region) {
		if (!strcmp(region, snda_ecs_region_huadong1)) {
			host = snda_ecs_huadong1_hostname;
		} else if (!strcmp(region, snda_ecs_region_huabei1)) {
			host = snda_ecs_huabei1_hostname;
		}
	}

	ptr += sprintf(ptr, "%s%s:%d", prefix, host, port);
	if (resource) {
		sprintf(ptr, "%s", resource);
	} else {
		sprintf(ptr, "%s", "/");
	}

	return urlbuff;
}

char* create_date_header(char* dateheaderbuff, char* datebuff) {
	sprintf(dateheaderbuff, "Date: %s", datebuff);
	return dateheaderbuff;
}

char* create_authorization_header(char* authbuff,
		const char* accesskey,
		const char * secretkey,
		const char* msgtosignature)
{
	char sha1buff[S_SNDA_ECS_SHA1_LEN] = { 0 };
	int sha1_len = hmac_sha1(secretkey, msgtosignature, strlen(msgtosignature), sha1buff);

	assert (sha1_len == S_SNDA_ECS_SHA1_LEN);

	char* signature = base64_encode(sha1buff, sha1_len);

	sprintf(authbuff, "Authorization: SNDA %s:%s", accesskey, signature);

	free(signature);

	return authbuff;
}

char* get_current_date(char* datebuff)
{
    time_t now = time(0);
    sprintf(datebuff, "%s", asctime(gmtime(&now)));
	int offset = strlen(datebuff);
	if (datebuff[offset - 1] == '\n') {
		offset -= 1;
	}
	sprintf(datebuff + offset, "%s", " UTC");
	return datebuff;
}

char* basic_msg_to_signature(char* msgtosignature,
		const char* httpverb,
		const char* contentmd5,
		const char* contenttype,
		const char* date,
		const char* canonicalizedheaders,
		const char* urlresource)
{
	sprintf(msgtosignature, "%s\n", httpverb);
	if (contentmd5) {
		sprintf(msgtosignature + strlen(msgtosignature), "%s", contentmd5);
	}
	sprintf(msgtosignature + strlen(msgtosignature), "\n");

	if (contenttype) {
		sprintf(msgtosignature + strlen(msgtosignature), "%s", contenttype);
	}
	sprintf(msgtosignature + strlen(msgtosignature), "\n");

	sprintf(msgtosignature + strlen(msgtosignature), "%s\n", date);

	if (canonicalizedheaders) {
		sprintf(msgtosignature + strlen(msgtosignature), "%s", canonicalizedheaders);
	}

	sprintf(msgtosignature + strlen(msgtosignature), "%s", urlresource);

	return msgtosignature;
}


size_t snda_ecs_get_server_response_body(void *ptr, size_t size, size_t number, void *stream)
{
	SNDAECSServerResponseBody *res = (SNDAECSServerResponseBody *)stream;
	int comedatasize = size * number;
	// the first day, res->retbodyremain is init to be 0 and res->retbody is set to be NULL.
	if (res->retbodyremain < comedatasize) {
		int moresize = S_SNDA_ECS_RET_BODY_ONE_TIME_ALLOCATE_LEN > comedatasize ?
				S_SNDA_ECS_RET_BODY_ONE_TIME_ALLOCATE_LEN :
				comedatasize;
		int nextsize = res->retbodysize + moresize;
		res->retbody = realloc(res->retbody, nextsize);
		res->retbodyremain = moresize;
	}
    memcpy(res->retbody + res->retbodysize, ptr, comedatasize);
    res->retbodysize += comedatasize;
    res->retbodyremain -= comedatasize;
    return comedatasize;
}

size_t snda_ecs_put_xml_body(void *ptr, size_t size, size_t nmemb, void *stream)
{
	SNDAECSReadBuff *buff = (SNDAECSReadBuff*)stream;

	long remain = buff->datasize - buff->consumed;
    if (remain == 0) {
    	return 0;
    }

    long thistimeconsumed = remain > nmemb * size ? nmemb * size : remain;
    memcpy(ptr, (char*)(buff->databuff) + buff->consumed,  thistimeconsumed);
    buff->consumed += thistimeconsumed;

    return thistimeconsumed;
}

size_t snda_ecs_put_object_body(void *ptr, size_t size, size_t nmemb, void *stream) {
	SNDAECSReadBuff *buff = (SNDAECSReadBuff*)stream;

	long remain = buff->datasize - buff->consumed;
	if (remain == 0) {
		return 0;
	}

	long thistimeconsumed = remain > nmemb * size ? nmemb * size : remain;

	FILE* fd = (FILE*)(buff->databuff);

	long actualconsumed = fread(ptr, sizeof(char), thistimeconsumed, fd);
	buff->consumed += actualconsumed;
	return actualconsumed;
}

size_t snda_ecs_write_fun(void *ptr, size_t size, size_t number, void *stream) {
	int comedatasize = size * number;
	FILE* output = (FILE*) stream;
	size_t actualwrite = 0;
	do {
		actualwrite += fwrite(ptr + actualwrite, sizeof(char),
				comedatasize - actualwrite, output);
	} while (actualwrite < comedatasize);

	return comedatasize;
}

void snda_ecs_set_handler_attributes(
		SNDAECSHandler* handler, struct curl_slist* headers, const char* url,
		CallbackFunPtr readfun, void* readerptr, long readerdatasize,
		CallbackFunPtr writefun, void* writerptr, SNDAECSHandleType type,
		SNDAECSFollowLocation followlocation, long maxredirects)
{
	curl_easy_setopt(handler->handler, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(handler->handler, CURLOPT_URL, url);

	curl_easy_setopt(handler->handler, CURLOPT_WRITEFUNCTION, writefun);
	curl_easy_setopt(handler->handler, CURLOPT_WRITEDATA, writerptr);

	if (followlocation == SNDA_ECS_FOLLOW_LOCATION) {
		curl_easy_setopt(handler->handler, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(handler->handler, CURLOPT_MAXREDIRS, maxredirects);
	}

	switch (type) {
	case SNDA_ECS_PUT_RESPONSE_HEAD:
		curl_easy_setopt(handler->handler, CURLOPT_HEADER, 1);
	case SNDA_ECS_PUT:
		curl_easy_setopt(handler->handler, CURLOPT_READFUNCTION, readfun);
		curl_easy_setopt(handler->handler, CURLOPT_READDATA, readerptr);
		curl_easy_setopt(handler->handler, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(handler->handler, CURLOPT_INFILESIZE_LARGE, (curl_off_t)readerdatasize);
		break;
	case SNDA_ECS_GET:
		curl_easy_setopt(handler->handler, CURLOPT_HTTPGET, 1L);
		break;
	case SNDA_ECS_DELETE:
		curl_easy_setopt(handler->handler, CURLOPT_CUSTOMREQUEST, "DELETE");
		break;
	case SNDA_ECS_HEAD:
		curl_easy_setopt(handler->handler, CURLOPT_HEADER, 1);
		curl_easy_setopt(handler->handler, CURLOPT_NOBODY, 1);
		curl_easy_setopt(handler->handler, CURLOPT_CUSTOMREQUEST, "HEAD");
		break;
	case SNDA_ECS_POST:
		curl_easy_setopt(handler->handler, CURLOPT_CUSTOMREQUEST, "POST");
		curl_easy_setopt(handler->handler, CURLOPT_READFUNCTION, readfun);
		curl_easy_setopt(handler->handler, CURLOPT_READDATA, readerptr);
		curl_easy_setopt(handler->handler, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(handler->handler, CURLOPT_INFILESIZE_LARGE, (curl_off_t)readerdatasize);
		break;
	default:
		break;
	}
}

static int snda_ecs_str_comparator(const void* left, const void* right) {
	return strcmp((*((SNDAECSKVList* const *)left))->key, (*((SNDAECSKVList* const *)right))->key);
}

static SNDAECSKVList* snda_ecs_key_to_lower(const SNDAECSKVList *left) {
	if (!left) {
		return NULL;
	}

	SNDAECSKVList* head = 0;
	SNDAECSKVList* cur = 0;
	const SNDAECSKVList* p = left;
	for (; p; p = p->next) {
		if (!cur) {
			cur = snda_ecs_init_k_v_list();
			head = cur;
		} else {
			cur->next = snda_ecs_init_k_v_list();
			cur = cur->next;
		}
		cur->key = calloc(strlen(p->key) + 1, sizeof(char));
		snda_ecs_to_lower_string(p->key, cur->key);
		cur->value = calloc(strlen(p->value) + 1, sizeof(char));
		memcpy(cur->value, p->value, strlen(p->value));
	}
	return head;
}

static char* snda_ecs_to_canonicalized_snda_headers(const SNDAECSKVList * header) {
	if (!header) {
		return NULL;
	}

	SNDAECSKVList* keylowercasepairs = snda_ecs_key_to_lower(header);

	int canonicalizedheaders = 0;
	SNDAECSKVList* pupet = keylowercasepairs;
	for (; pupet; pupet = pupet->next) {
		if (snda_ecs_is_start_with(pupet->key, snda_canonicalized_prefix)) {
			++canonicalizedheaders;
		}
	}

	if (!canonicalizedheaders) {
		snda_ecs_release_k_v_list(keylowercasepairs);
		return NULL;
	}



	int headermaxsize = 0;
	SNDAECSKVList** pairarr = (SNDAECSKVList**)malloc(canonicalizedheaders * sizeof(SNDAECSKVList*));
	SNDAECSKVList** p = pairarr;
	for (pupet = keylowercasepairs; pupet; pupet = pupet->next) {
		if (snda_ecs_is_start_with(pupet->key, snda_canonicalized_prefix)) {
			headermaxsize += strlen(pupet->key) + strlen(pupet->value) + 1;
			*(p++) = pupet;
		}
	}

	qsort(pairarr, canonicalizedheaders, sizeof(SNDAECSKVList*), snda_ecs_str_comparator);

	char *headerstring = calloc(headermaxsize + 1, sizeof(char));
	char *lastkey = 0;
	int i = 0;
	for (; i < canonicalizedheaders; ++i) {
		if (!lastkey) {
			sprintf(headerstring, "%s:%s", pairarr[i]->key, pairarr[i]->value);
			lastkey = pairarr[i]->key;
			continue;
		}

		if (!strcmp(lastkey, pairarr[i]->key)) {
			sprintf(headerstring + strlen(headerstring), ",%s", pairarr[i]->value);
			continue;
		}

		sprintf(headerstring + strlen(headerstring), "\n%s:%s", pairarr[i]->key, pairarr[i]->value);
		lastkey = pairarr[i]->key;
	}
	headerstring[strlen(headerstring)] = '\n';

	free(pairarr);
	snda_ecs_release_k_v_list(keylowercasepairs);

	return headerstring;
}

static SNDAECSStringList* snda_ecs_create_user_object_headers(const SNDAECSUserObjectMeta const* userobjectmeta)
{
	SNDAECSStringList* stringlist = snda_ecs_init_string_list();
	SNDAECSStringList* cur = stringlist;

	if (userobjectmeta->contentmd5) {
		const char* contentmd5prefix = "Content-MD5: ";
		cur->string = calloc(strlen(contentmd5prefix) + strlen(userobjectmeta->contentmd5) + 1, sizeof(char));
		sprintf(cur->string, "%s%s", contentmd5prefix, userobjectmeta->contentmd5);
	}

	if (userobjectmeta->contenttype) {
		if (cur->string) {
			cur->next = snda_ecs_init_string_list();
			cur = cur->next;
		}
		const char* contenttypeprefix = "Content-Type: ";
		cur->string = calloc(strlen(contenttypeprefix) + strlen(userobjectmeta->contenttype) + 1, sizeof(char));
		sprintf(cur->string, "%s%s", contenttypeprefix, userobjectmeta->contenttype);
	}

	SNDAECSKVList * p = userobjectmeta->usermetas;
	for (; p; p = p->next) {
		if (p->key) {
			if (cur->string) {
				cur->next = snda_ecs_init_string_list();
				cur = cur->next;
			}
			cur->string = calloc(strlen(p->key) + strlen(": ") + strlen(p->value) + 1, sizeof(char));
			sprintf(cur->string, "%s: %s", p->key, p->value);
		}
	}

	if (!(stringlist->string)) {
		snda_ecs_release_string_list(stringlist);
		stringlist = 0;
	}
	return stringlist;
}

void snda_ecs_get_common_opt_attributes(const char* accesskey,
		 const char* secretkey, const char* bucketname, const char* subresource, const char* subresourceneedtosign,
		 const char* contentmd5, const char* contenttype, const char* canonicalizedheaders,
		 const char* region, int ssl, SNDAECSHandleType type,
		 struct curl_slist** headersptr, char* url)
{
	const char* root = "/";
	int urlresourcebuffsize = strlen(root) + strlen(bucketname) + 1;

	int resourcetosignsize = urlresourcebuffsize;
	if (subresourceneedtosign) {
		resourcetosignsize += strlen(subresourceneedtosign);
	}

	int urlresourcebuffsizefull = urlresourcebuffsize;
	if (subresource) {
		urlresourcebuffsizefull += strlen(subresource);
	}

	char urlresource[urlresourcebuffsize];
	sprintf(urlresource, "/%s", bucketname);

	char resourcetosign[resourcetosignsize];
	sprintf(resourcetosign, "%s", urlresource);
	if (subresourceneedtosign) {
		sprintf(resourcetosign + strlen(resourcetosign), "%s", subresourceneedtosign);
	}

	char urlresourcefull[urlresourcebuffsizefull];
	sprintf(urlresourcefull, "%s", urlresource);
	if (subresource) {
		sprintf(urlresourcefull + strlen(urlresourcefull), "%s", subresource);
	}

	create_url(url, region, urlresourcefull, ssl);

	char* httpverb = "PUT";
	switch (type) {
	case SNDA_ECS_GET:
		httpverb = "GET";
		break;
	case SNDA_ECS_DELETE:
		httpverb = "DELETE";
		break;
	case SNDA_ECS_HEAD:
		httpverb = "HEAD";
		break;
	case SNDA_ECS_POST:
		httpverb = "POST";
		break;
	default:
		break;
	}

	char date[S_SNDA_ECS_DATE_LEN];
	get_current_date(date);

	char signature[S_SNDA_ECS_BASIC_MSG_TO_SIGNATURE_LEN];
	basic_msg_to_signature(signature, httpverb, contentmd5, contenttype, date, canonicalizedheaders, resourcetosign);

	char authorheader[S_SNDA_ECS_AUTH_LEN];
	create_authorization_header(authorheader, accesskey, secretkey, signature);

	char dateheader[S_SNDA_ECS_DATE_HEADER_LEN];
	create_date_header(dateheader, date);

	struct curl_slist* headers = curl_slist_append(NULL, dateheader);
	headers = curl_slist_append(headers, authorheader);
	*headersptr = headers;
}


SNDAECSErrorCode snda_ecs_common_opt(SNDAECSHandler* handler, const char* accesskey, const char* secretkey,
		const char* bucketname, const char* region, const char* subresource, const char* subresourcetosign, int ssl, SNDAECSByteRange* byterange,
		const SNDAECSUserObjectMeta* userobjectmeta, SNDAECSHandleType type, SNDAECSFollowLocation followlocation, long maxredirects,
		CallbackFunPtr readfun, void* inputstream, long inputlength, CallbackFunPtr writefun, void* outputstream,
		SNDAECSResult* ret)
{
	char* contentmd5 = 0;
	char* contenttype = 0;
	char* canonicalizedheaders = 0;
	if (userobjectmeta) {
		contentmd5 = userobjectmeta->contentmd5;
		contenttype = userobjectmeta->contenttype;
		canonicalizedheaders = snda_ecs_to_canonicalized_snda_headers(userobjectmeta->usermetas);
	}

	char url[S_SNDA_ECS_MAX_URL_LEN];
	struct curl_slist* headers = 0;
	snda_ecs_get_common_opt_attributes(accesskey, secretkey,
			bucketname, subresource, subresourcetosign,
			contentmd5, contenttype, canonicalizedheaders,
			region, ssl, type,
			&headers, url);

	if (byterange) {
		char byterangeheader[S_SNDA_ECS_BYTE_RANGE_HEADER_LEN];
		sprintf(byterangeheader, "Range: bytes=%ld-%ld", byterange->first, byterange->last);
		headers = curl_slist_append(headers, byterangeheader);
	}

	SNDAECSStringList* p = 0;
	if (userobjectmeta) {
		p = snda_ecs_create_user_object_headers(userobjectmeta);
		for (; p; p = p->next) {
			if (p->string) {
				headers = curl_slist_append(headers, p->string);
			}
		}
	}

	snda_ecs_set_handler_attributes(handler,
				headers, url,
				readfun, inputstream, inputlength,
				writefun,
				outputstream,
				type, followlocation, maxredirects);

	CURLcode retcode = curl_easy_perform(handler->handler);

	curl_slist_free_all(headers);
	snda_ecs_release_string_list(p);
	snda_ecs_free_char_ptr(canonicalizedheaders);

	if (retcode != CURLE_OK) {
		snda_ecs_set_error_info(handler, ret);
		return SNDA_ECS_HANDLER_ERROR;
	}

	curl_easy_getinfo(handler->handler, CURLINFO_RESPONSE_CODE, &(ret->serverresponse->httpcode));

	return SNDA_ECS_SUCCESS;
}

