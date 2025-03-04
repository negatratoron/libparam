/*
 * vmem_client_slash.c
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */


#include <stdio.h>
#include <sys/stat.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include <vmem/vmem_client.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>

static int vmem_client_slash_download(struct slash *slash)
{

	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
    unsigned int version = 1;
	unsigned int offset = 0;
	unsigned int use_rdp = 1;

    optparse_t * parser = optparse_new("download", "<address> <length base10 or base16> <file>");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version (default = 1)");
	optparse_add_unsigned(parser, 'o', "offset", "NUM", 0, &offset, "byte offset in file (default = 0)");
	optparse_add_unsigned(parser, 'r', "use_rdp", "NUM", 0, &use_rdp, "rdp ack for each (default = 1)");

	rdp_opt_add(parser);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	rdp_opt_set();

	/* Expect address */
	if (++argi >= slash->argc) {
		printf("missing address\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * endptr;
	uint64_t address = strtoul(slash->argv[argi], &endptr, 16);
	if (*endptr != '\0') {
		printf("Failed to parse address\n");
        optparse_del(parser);
		return SLASH_EUSAGE;
	}

	/* Expect length */
	if (++argi >= slash->argc) {
		printf("missing length\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	uint32_t length = strtoul(slash->argv[argi], &endptr, 10);
	if (*endptr != '\0') {
		length = strtoul(slash->argv[argi], &endptr, 16);
		if (*endptr != '\0') {
			printf("Failed to parse length in base 10 or base 16\n");
            optparse_del(parser);
			return SLASH_EUSAGE;
		}
	}

	/* Expect filename */
	if (++argi >= slash->argc) {
		printf("missing filename\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * file;
	file = slash->argv[argi];

	printf("Download from %u addr 0x%"PRIX64" to %s with timeout %u version %u\n", node, address, file, timeout, version);

	/* Allocate memory for reply */
	char * data = malloc(length);

	vmem_download(node, timeout, address, length, data, version, use_rdp);

	/* Open file (truncate or create) */
	FILE * fd = fopen(file, "w+");
	if (fd == NULL) {
		free(data);
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Write data */
	int written = fwrite(data, 1, length, fd);
	fclose(fd);
	free(data);

	printf("wrote %u bytes to %s\n", written, file);

    optparse_del(parser);

	rdp_opt_reset();

	return SLASH_SUCCESS;
}
slash_command(download, vmem_client_slash_download, "<address> <length> <file>", "Download from VMEM to FILE");

static int vmem_client_slash_upload(struct slash *slash)
{

	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
    unsigned int version = 1;
	unsigned int offset = 0;

    optparse_t * parser = optparse_new("upload", "<file> <address>");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version (default = 1)");
	optparse_add_unsigned(parser, 'o', "offset", "NUM", 0, &offset, "byte offset in file (default = 0)");

	rdp_opt_add(parser);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	rdp_opt_set();

	/* Expect filename */
	if (++argi >= slash->argc) {
		printf("missing filename\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * file;
	file = slash->argv[argi];

	/* Expect address */
	if (++argi >= slash->argc) {
		printf("missing address\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * endptr;
	uint64_t address = strtoul(slash->argv[argi], &endptr, 16);
	if (*endptr != '\0') {
		printf("Failed to parse address\n");
        optparse_del(parser);
		return SLASH_EUSAGE;
	}

	printf("Upload from %s to node %u addr 0x%"PRIX64" with timeout %u, version %u\n", file, node, address, timeout, version);

	/* Open file */
	FILE * fd = fopen(file, "r");
	if (fd == NULL){
    	optparse_del(parser);
		return SLASH_EINVAL;
  }

	/* Read size */
	struct stat file_stat;
	stat(file, &file_stat);

	fseek(fd, offset, SEEK_SET);

	/* Copy to memory */
	char * data = malloc(file_stat.st_size);
	int size = fread(data, 1, file_stat.st_size - offset, fd);
	fclose(fd);

	address += offset;

	printf("File size %ld, offset %d, to upload %d to address %lx\n", file_stat.st_size, offset, size, address);

	csp_hex_dump("File head", data, 256);

	printf("Size %u\n", size);

	vmem_upload(node, timeout, address, data, size, version);

    if(data){
        free(data);
    }
    optparse_del(parser);

	rdp_opt_reset();

	return SLASH_SUCCESS;
}
slash_command(upload, vmem_client_slash_upload, "<file> <address>", "Upload from FILE to VMEM");

unsigned int rdp_dfl_window = 3;
unsigned int rdp_dfl_conn_timeout = 10000;
unsigned int rdp_dfl_packet_timeout = 5000;
unsigned int rdp_dfl_delayed_acks = 1;
unsigned int rdp_dfl_ack_timeout = 2000;
unsigned int rdp_dfl_ack_count = 2;

static int vmem_client_rdp_options(struct slash *slash) {

    optparse_t * parser = optparse_new("rdp options", "");
    optparse_add_help(parser);

    csp_rdp_get_opt(&rdp_dfl_window, &rdp_dfl_conn_timeout, &rdp_dfl_packet_timeout, &rdp_dfl_delayed_acks, &rdp_dfl_ack_timeout, &rdp_dfl_ack_count);

	printf("Current RDP options window: %u, conn_timeout: %u, packet_timeout: %u, ack_timeout: %u, ack_count: %u\n", 
        rdp_dfl_window, rdp_dfl_conn_timeout, rdp_dfl_packet_timeout, rdp_dfl_ack_timeout, rdp_dfl_ack_count);

	optparse_add_unsigned(parser, 'w', "window", "NUM", 0, &rdp_dfl_window, "rdp window (default = 3 packets)");
	optparse_add_unsigned(parser, 'c', "conn_timeout", "NUM", 0, &rdp_dfl_conn_timeout, "rdp connection timeout in ms (default = 10 seconds)");
	optparse_add_unsigned(parser, 'p', "packet_timeout", "NUM", 0, &rdp_dfl_packet_timeout, "rdp packet timeout in ms (default = 5 seconds)");
	optparse_add_unsigned(parser, 'k', "ack_timeout", "NUM", 0, &rdp_dfl_ack_timeout, "rdp max acknowledgement interval in ms (default = 2 seconds)");
	optparse_add_unsigned(parser, 'a', "ack_count", "NUM", 0, &rdp_dfl_ack_count, "rdp ack for each (default = 2 packets)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	printf("Setting RDP options window: %u, conn_timeout: %u, packet_timeout: %u, ack_timeout: %u, ack_count: %u\n", 
        rdp_dfl_window, rdp_dfl_conn_timeout, rdp_dfl_packet_timeout, rdp_dfl_ack_timeout, rdp_dfl_ack_count);

	csp_rdp_set_opt(rdp_dfl_window, rdp_dfl_conn_timeout, rdp_dfl_packet_timeout, 1, rdp_dfl_ack_timeout, rdp_dfl_ack_count);

	return SLASH_SUCCESS;
}
slash_command_sub(rdp, opt, vmem_client_rdp_options, NULL, "Set RDP options to use in stream and file transfers");

unsigned int rdp_tmp_window;
unsigned int rdp_tmp_conn_timeout;
unsigned int rdp_tmp_packet_timeout;
unsigned int rdp_tmp_delayed_acks;
unsigned int rdp_tmp_ack_timeout;
unsigned int rdp_tmp_ack_count;

void rdp_opt_add(optparse_t * parser) {

	rdp_tmp_window = rdp_dfl_window;
	rdp_tmp_conn_timeout = rdp_dfl_conn_timeout;
	rdp_tmp_packet_timeout = rdp_dfl_packet_timeout;
	rdp_tmp_delayed_acks = rdp_dfl_delayed_acks;
	rdp_tmp_ack_timeout = rdp_dfl_ack_timeout;
	rdp_tmp_ack_count = rdp_dfl_ack_count;

	optparse_add_unsigned(parser, 'w', "window", "NUM", 0, &rdp_tmp_window, "rdp window (default = 3 packets)");
	optparse_add_unsigned(parser, 'c', "conn_timeout", "NUM", 0, &rdp_tmp_conn_timeout, "rdp connection timeout (default = 10000)");
	optparse_add_unsigned(parser, 'p', "packet_timeout", "NUM", 0, &rdp_tmp_packet_timeout, "rdp packet timeout (default = 5000)");
	optparse_add_unsigned(parser, 'k', "ack_timeout", "NUM", 0, &rdp_tmp_ack_timeout, "rdp max acknowledgement interval (default = 2000)");
	optparse_add_unsigned(parser, 'a', "ack_count", "NUM", 0, &rdp_tmp_ack_count, "rdp ack for each (default = 2 packets)");
}

void rdp_opt_set() {

	csp_rdp_set_opt(rdp_tmp_window, rdp_tmp_conn_timeout, rdp_tmp_packet_timeout, rdp_tmp_delayed_acks, rdp_tmp_ack_timeout, rdp_tmp_ack_count);

	printf("Using RDP options window: %u, conn_timeout: %u, packet_timeout: %u, ack_timeout: %u, ack_count: %u\n", 
        rdp_tmp_window, rdp_tmp_conn_timeout, rdp_tmp_packet_timeout, rdp_tmp_ack_timeout, rdp_tmp_ack_count);
}

void rdp_opt_reset() {

	csp_rdp_set_opt(rdp_dfl_window, rdp_dfl_conn_timeout, rdp_dfl_packet_timeout, rdp_dfl_delayed_acks, rdp_dfl_ack_timeout, rdp_dfl_ack_count);
}
