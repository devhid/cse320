#include "protocol.h"
#include "csapp.h"
#include "debug.h"

int proto_send_packet(int fd, bvd_packet_header *hdr, void *payload) {
    /*debug("--- Packet Header Information ---");
    debug("Packet Type: %i", hdr->type);
    debug("Payload Length: %i", hdr->payload_length);
    debug("Message Id: %i", hdr->msgid);
    debug("Timestamp Sec: %i", hdr->timestamp_sec);
    debug("Timestamp Nsec: %i", hdr->timestamp_nsec);*/

    // Save the old payload_length before it is converted.
    uint32_t payload_length = hdr->payload_length;

    // Perform conversion from host-byte order to network-byte order.
    hdr->payload_length = htonl(hdr->payload_length);
    hdr->msgid = htonl(hdr->msgid);
    hdr->timestamp_sec = htonl(hdr->timestamp_sec);
    hdr->timestamp_nsec = htonl(hdr->timestamp_nsec);

    // Write the input error to the file indicated by the file descriptor.
    if(write(fd, hdr, sizeof(bvd_packet_header)) < sizeof(bvd_packet_header)) {
        debug("proto_send_packet(): write() returned -1 when writing header.");
        return -1;
    }

    // If a payload exists, and the length is nonzero,
    // write the payload to the file indicated by file descriptor.
    if(payload != NULL && payload_length != 0) {
        if(write(fd, payload, payload_length) < payload_length) {
            debug("proto_send_packet(): write() returned -1 when writing payload.");
            return -1;
        }
    }

    // No error has occurred so we return 0.
    return 0;
}

int proto_recv_packet(int fd, bvd_packet_header *hdr, void **payload) {
    // Read in the struct information for the packet header.
    if(read(fd, hdr, sizeof(bvd_packet_header)) < sizeof(bvd_packet_header)) {
        debug("proto_recv_packet(): read() returned -1 when reading header.");
        return -1;
    }

    // Fill in the struct fields from header to hdr and
    // convert any multi-byte information to host-byte order.
    hdr->payload_length = ntohl(hdr->payload_length);
    hdr->msgid = ntohl(hdr->msgid);
    hdr->timestamp_sec = ntohl(hdr->timestamp_sec);
    hdr->timestamp_nsec = ntohl(hdr->timestamp_nsec);

    // If the payload pointer exists and the length is nonzero,
    // read in the payload.
    if(payload != NULL && hdr->payload_length != 0) {
        // Allocate memory to store the payload.
        char *msg = NULL;
        if((msg = calloc(hdr->payload_length, sizeof(char))) == NULL) {
            debug("proto_recv_packet(): calloc() returned NULL.");
            return -1;
        }

        // Read in the payload to msg.
        if(read(fd, msg, hdr->payload_length) < hdr->payload_length) {
            debug("proto_recv_packet(): read() returned -1 when reading payload.");
            free(msg);
            return -1;
        }

        // Store the msg in the address pointed to by payload.
        *payload = msg;
    }

    // No error has occurred so we return 0.
    return 0;
}
