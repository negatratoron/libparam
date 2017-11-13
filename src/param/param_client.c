/*
 * param_client.c
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <malloc.h>
#include <inttypes.h>
#include <param/param.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <csp/csp_endian.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_client.h>
#include <param/param_queue.h>

typedef void (*param_transaction_callback_f)(csp_packet_t *response);

static void param_transaction_callback_pull(csp_packet_t *response) {
	csp_hex_dump("pull response", response->data, response->length);
	param_queue_t * queue = param_queue_create(&response->data[2], response->length - 2, response->length - 2, PARAM_QUEUE_TYPE_SET);
	param_queue_print(queue);
	param_queue_apply(queue);
	param_queue_destroy(queue);
	csp_buffer_free(response);
}

static int param_transaction(csp_packet_t *packet, int host, int timeout, param_transaction_callback_f callback) {

	csp_hex_dump("transaction", packet->data, packet->length);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, host, PARAM_PORT_SERVER, 0, CSP_O_CRC32);
	if (conn == NULL) {
		csp_buffer_free(packet);
		return -1;
	}

	if (!csp_send(conn, packet, 0)) {
		csp_close(conn);
		csp_buffer_free(packet);
		return -1;
	}

	if (timeout == -1) {
		csp_close(conn);
		return -1;
	}

	while((packet = csp_read(conn, timeout)) != NULL) {
		if (packet == NULL) {
			csp_close(conn);
			return -1;
		}

		int end = (packet->data[1] == PARAM_FLAG_END);

		if (callback) {
			callback(packet);
		} else {
			csp_buffer_free(packet);
		}

		if (end) {
			break;
		}

	}

	//csp_hex_dump("transaction response", packet->data, packet->length);

	csp_close(conn);
	return 0;
}


#if 0
int param_pull_all(int verbose, int host, int timeout) {
	csp_packet_t *packet = csp_buffer_get(256);
	if (packet == NULL)
		return -2;
	packet->data[0] = PARAM_PULL_ALL_REQUEST;
	packet->data[1] = 0;

	packet = param_transaction(packet, host, timeout);
	if (packet == NULL) {
		printf("No response\n");
		return -1;
	}
}
#endif


int param_pull_queue(param_queue_t *queue, int verbose, int host, int timeout) {

	if ((queue == NULL) || (queue->used == 0))
		return 0;

	// TODO: include unique packet id?
	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_PULL_REQUEST;
	packet->data[1] = 0;

	memcpy(&packet->data[2], queue->buffer, queue->used);

	packet->length = queue->used + 2;
	return param_transaction(packet, host, timeout, param_transaction_callback_pull);

}


int param_pull_single(param_t *param, int verbose, int host, int timeout) {

	// TODO: include unique packet id?
	csp_packet_t * packet = csp_buffer_get(256);
	packet->data[0] = PARAM_PULL_REQUEST;
	packet->data[1] = 0;

	param_queue_t * queue = param_queue_create(&packet->data[2], 256 - 2, 0, PARAM_QUEUE_TYPE_GET);
	param_queue_add(queue, param, NULL);

	packet->length = queue->used + 2;
	int result = param_transaction(packet, host, timeout, param_transaction_callback_pull);
	param_queue_destroy(queue);
	return result;
}


int param_push_queue(param_queue_t *queue, int verbose, int host, int timeout) {

	if ((queue == NULL) || (queue->used == 0))
		return 0;

	// TODO: include unique packet id?
	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_PUSH_REQUEST;
	packet->data[1] = 0;

	memcpy(&packet->data[2], queue->buffer, queue->used);

	packet->length = queue->used + 2;
	int result = param_transaction(packet, host, timeout, NULL);

	if (result < 0) {
		return -1;
	}

	param_queue_print(queue);
	param_queue_apply(queue);

	return 0;
}

int param_push_single(param_t *param, void *value, int verbose, int host, int timeout) {

	// TODO: include unique packet id?
	csp_packet_t * packet = csp_buffer_get(256);
	packet->data[0] = PARAM_PUSH_REQUEST;
	packet->data[1] = 0;

	param_queue_t * queue = param_queue_create(&packet->data[2], 256 - 2, 0, PARAM_QUEUE_TYPE_SET);
	param_queue_add(queue, param, value);

	packet->length = queue->used + 2;
	int result = param_transaction(packet, host, timeout, NULL);

	if (result < 0) {
		param_queue_destroy(queue);
		return -1;
	}

	param_queue_apply(queue);
	param_queue_destroy(queue);

	return 0;
}

