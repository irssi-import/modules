#include "common.h"
#include "irc_crypt.h"

#define MODULE_NAME "idea"

typedef struct {
	void (*orig_send_message)(SERVER_REC *server, const char *target,
				  const char *msg, int target_type);
} MODULE_SERVER_REC;

/* CRC */
char *irc_crc_string(const char *str);
char *irc_crc_buffer(const char *buf, int len);
unsigned int irc_crc_string_numeric(const char *str);
unsigned int irc_crc_buffer_numeric(const char *buf, int len);
int irc_check_crc_string(const char *str, const char *crc);
int irc_check_crc_buffer(const char *str, int len, const char *crc);
int irc_check_crc_string_numeric(const char *str, unsigned int crc);
int irc_check_crc_buffer_numeric(const char *str, int len, unsigned int crc);

/* B64 */
char *b64_encode_buffer(const char *buf, int *len);
char *b64_decode_buffer(const char *buf, int *len);

/* CRYPT */
char *irc_encrypt_buffer(const char *key, const char *str, int *len);
char *irc_decrypt_buffer(const char *key, const char *str, int *len, int version);
char *irc_key_fingerprint(const char *key, int version);

/* irc_idea_v[123] */
char *irc_idea_key_fingerprint_v1(const char *key_str);
unsigned short *irc_idea_key_expand_v1(const char *key_str, int key_str_len);
char *irc_idea_key_fingerprint_v2(const char *key_str);
unsigned short *irc_idea_key_expand_v2(const char *key_str, int key_str_len);
char *irc_idea_key_fingerprint_v3(const char *key_str);
unsigned short *irc_idea_key_expand_v3(const char *key_str, int key_str_len);
