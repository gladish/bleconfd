//
// Copyright [2018] [Comcast NBCUniversal]
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <string.h>
#include <errno.h>
#include <string.h>

#include <cJSON.h>
#include "rpclogger.h"
#include "ecdh.h"

#define _DEBUG_CRYPTO_ 0

typedef unsigned char byte_t;


typedef struct
{
  byte_t* data;
  int     length;
} byteArray_t;

void
baInit(
  byteArray_t* v,
  int length);

void
baClear(
  byteArray_t* v);

#if _DEBUG_CRYPTO_
void
baPrint(
  byteArray_t const* v,
  char const* name);
#endif

char*
baToString(
  byteArray_t const* v);

int
baFromBase64(
  byteArray_t* decoded_data,
  char const* encoded_data);

int
ECDH_SharedKeyDerive(
  EVP_PKEY* private_key,
  EVP_PKEY* peer_key,
  byteArray_t* secret_key);

int
AESDecrypt(
  byteArray_t const* key,
  byteArray_t const* iv,
  byteArray_t const* encrypted_data,
  byteArray_t*       plain_data);

EVP_PKEY*
ECC_ReadPrivateKeyFromFile(
  char const* fname);

EVP_PKEY*
ECC_ReadPublicKeyFromPEM(
  char const* pem_data);

EVP_PKEY*
ECC_ReadPublicKeyFromFile(
  char const* fname);

int
ECDH_Decrypt(
  char const* private_key_path,
  char const* peer_public_key,
  char const* iv,
  char const* cipher_text,
  char** clear_text,
  int* clear_text_length);

static char const*
CRYPTO_Error();

#if 0
int main(int argc, char* argv[])
{
  FILE*         fin;
  cJSON*        reply;
  cJSON*        wifi_settings;
  byteArray_t   buff;

  fin = NULL;
  reply = NULL;
  wifi_settings = NULL;
  baInit(&buff, 2048);

  // do this on startup
  ERR_load_crypto_strings();

  fin = fopen(argv[1], "r");
  fread(buff.data, 1, buff.length, fin);
  fclose(fin);

  reply = cJSON_Parse((char const *)buff.data);
  ECDH_DecryptWiFiSettings(reply, &wifi_settings);

  if (wifi_settings)
  {
    char* s = cJSON_Print(wifi_settings);
    printf("%s\n", s);
    free(s);
    cJSON_Delete(wifi_settings);
  }

  baClear(&buff);
  cJSON_Delete(reply);
  return 0;
}
#endif

int
ECDH_Decrypt(
  char const* private_key_path,
  char const* peer_public_key,
  char const* iv,
  char const* cipher_text,
  char** clear_text,
  int* clear_text_length)
{
  int           ret;
  char*         p;

  EVP_PKEY*     private_key;
  EVP_PKEY*     peer_key;

  byteArray_t   shared_secret;
  byteArray_t   encdata;
  byteArray_t   ivector;
  byteArray_t   encrypted;
  byteArray_t   decrypted;

  ret = 0;
  p = NULL;
  private_key = NULL;
  peer_key = NULL;

  baInit(&shared_secret, 0);
  baInit(&encdata, 0);
  baInit(&decrypted, 0);

  baFromBase64(&ivector, iv);
  baFromBase64(&encrypted, cipher_text);

  // read my private key
  private_key = ECC_ReadPrivateKeyFromFile(private_key_path);
  if (!private_key)
  {
    XLOG_ERROR("failed to read private key from '%s'", private_key_path);
    ret = 0;
    goto error;
  }

  // parse peer's public key
  peer_key = ECC_ReadPublicKeyFromPEM(peer_public_key);
  if (!peer_key)
  {
    XLOG_ERROR("failed to read public key from pem data");
    XLOG_INFO(" --------- PEER PUBLIC KEY -----------\n%s",
      peer_public_key);
    ret = 0;
    goto error;
  }

  // create shared/derived key
  ret = ECDH_SharedKeyDerive(private_key, peer_key, &shared_secret);
  if (!ret)
  {
    XLOG_ERROR("failed to derive shared key");
    goto error;
  }

  // use shared/derived key to decrypt payload
  ret = AESDecrypt(&shared_secret, &ivector, &encrypted, &decrypted);
  if (!ret)
  {
    XLOG_ERROR("failed to decrypt data");
    goto error;
  }

  // copy payload to output parameters
  p = (char *) malloc(sizeof(byte_t) * (decrypted.length + 1));
  memcpy(p, decrypted.data, decrypted.length);
  p[decrypted.length] = '\0';

  *clear_text = p;
  *clear_text_length = decrypted.length;

  // cleanup
error:
  baClear(&shared_secret);
  baClear(&encdata);
  baClear(&ivector);
  baClear(&encrypted);
  baClear(&decrypted);

  if (private_key)
    EVP_PKEY_free(private_key);

  if (peer_key)
    EVP_PKEY_free(peer_key);

  return ret;
}

int
baFromBase64(byteArray_t* decoded_data, char const* encoded_data)
{
  BIO*      b64;
  BIO*      buff;
  size_t    encoded_length;
  size_t    bytes_written;

  b64 = NULL;
  buff = NULL;
  encoded_length = 0;
  bytes_written = 0;

  if (!decoded_data)
  {
    XLOG_ERROR("no parameter sepcified to base64 decode into");
    return 0;
  }

  if (!encoded_data)
  {
    XLOG_ERROR("no parameter specified to base64 decode");
    return 0;
  }

  encoded_length = strlen(encoded_data);

  b64 = BIO_new(BIO_f_base64());
  buff = BIO_new_mem_buf(encoded_data, encoded_length);
  buff = BIO_push(b64, buff);
  BIO_set_flags(buff, BIO_FLAGS_BASE64_NO_NL);
  BIO_set_flags(buff, BIO_CLOSE);

  baInit(decoded_data, encoded_length);
  memset(decoded_data->data, 0, encoded_length);
  decoded_data->length = 0;

  bytes_written = BIO_read(buff, decoded_data->data, encoded_length);
  decoded_data->length = bytes_written;

  BIO_free_all(buff);

  return 0;
}

char*
baToString(
  byteArray_t const* v)
{
  int i;
  char* s;

  i = 0;
  s = (char *) malloc(v->length + 1);

  for (i = 0; i < v->length; ++i)
    s[i] = (char) v->data[i];

  s[v->length] = '\0';
  return s;
}

int
AESDecrypt(
  byteArray_t const* key,
  byteArray_t const* iv,
  byteArray_t const* encrypted_data,
  byteArray_t*       plain_data)
{
  int                 n;
  int                 ret;
  int                 length;
  EVP_CIPHER_CTX*     cipher_ctx;

  n = 0;
  ret = 0;
  length = 0;
  cipher_ctx = NULL;

  baInit(plain_data, encrypted_data->length);

  #if _DEBUG_CRYPTO_
  baPrint(key, "KEY");
  baPrint(iv, "IV");
  baPrint(encrypted_data, "ENCRYPTED DATA");
  #endif

  cipher_ctx = EVP_CIPHER_CTX_new();
  if (!cipher_ctx)
  {
    XLOG_ERROR("EVP_CIPHER_CTX_new failed. %s", CRYPTO_Error());
    baClear(plain_data);
    return 0;
  }

  ret = EVP_DecryptInit_ex(cipher_ctx, EVP_aes_256_cbc(), NULL, key->data, iv->data);
  if (!ret)
  {
    XLOG_ERROR("EVP_DecryptInit_ex failed. %s", CRYPTO_Error());
    EVP_CIPHER_CTX_free(cipher_ctx);
    return 0;
  }

#if 0
  EVP_CIPHER_CTX_set_padding(ctx.get(), 1);
  EVP_CIPHER_CTX_set_key_length(ctx.get(), 16);
#endif

  ret = EVP_DecryptUpdate(cipher_ctx, plain_data->data, &n, encrypted_data->data, encrypted_data->length);
  if (ret == 1)
  {
    length += n;
  }
  else
  {
    XLOG_ERROR("EVP_DecryptUpdate failed. %s", CRYPTO_Error());
    EVP_CIPHER_CTX_free(cipher_ctx);
    return 0;
  }

  ret = EVP_DecryptFinal_ex(cipher_ctx, (plain_data->data + length), &n);
  if (ret == 1)
  {
    length += n;
  }
  else
  {
    XLOG_ERROR("EVP_DecryptFinal_ex failed. %s", CRYPTO_Error());
    EVP_CIPHER_CTX_free(cipher_ctx);
    return 0;
  }

  if (length > 0 && length < plain_data->length)
  {
    plain_data->length = length;
    plain_data->data[length] = '\0';
  }

  EVP_CIPHER_CTX_free(cipher_ctx);
  return 1;
}

EVP_PKEY*
ECC_ReadPublicKeyFromPEM(
  char const* pem_data)
{
  EVP_PKEY*   public_key;
  BIO*        buff;

  // The only way I can figure out how to get openssl to parse an elliptical
  // key was to include headers, otherwise, the sequence of calls is 
  // different
  static char const pem_header[] = "-----BEGIN PUBLIC KEY-----";
  static char const pem_footer[] = "-----END PUBLIC KEY-----";

  public_key = NULL;
  buff = NULL;

  buff = BIO_new(BIO_s_mem());
  BIO_write(buff, pem_header, strlen(pem_header));
  BIO_write(buff, "\n", 1);
  BIO_write(buff, pem_data, strlen(pem_data));
  BIO_write(buff, "\n", 1);
  BIO_write(buff, pem_footer, strlen(pem_footer));

  public_key = PEM_read_bio_PUBKEY(buff, NULL, NULL, NULL);
  if (!public_key)
  {
    XLOG_ERROR("failed to parse PEM data for public key. %s", CRYPTO_Error());
  }
  BIO_free_all(buff);

  return public_key;
}

EVP_PKEY*
ECC_ReadPublicKeyFromFile(
  char const* fname)
{
  FILE*       fin;
  EVP_PKEY*   public_key;

  fin = NULL;
  public_key = NULL;

  if (!fname)
  {
    XLOG_ERROR("can't read public key from NULL filename");
    return NULL;
  }

  fin = fopen(fname, "r");
  if (!fin)
  {
    int err = errno;
    XLOG_ERROR("failed to open file to read public key '%s'. %d",
      fname, err);
    return NULL;
  }

  public_key = PEM_read_PUBKEY(fin, NULL, NULL, NULL);
  fclose(fin);

  return public_key;
}

EVP_PKEY*
ECC_ReadPrivateKeyFromFile(char const* fname)
{
  FILE*       fin;
  EVP_PKEY*   private_key;

  fin = NULL;
  private_key = NULL;

  fin = fopen(fname, "r");
  if (!fin)
  {
    int err = errno;
    XLOG_ERROR("failed to open file to read private key '%s'. %d",
      fname, err);
    return NULL;
  }

  private_key = PEM_read_PrivateKey(fin, NULL, NULL, NULL);
  fclose(fin);

  return private_key;
}

int
ECDH_SharedKeyDerive(
  EVP_PKEY* private_key,
  EVP_PKEY* peer_key,
  byteArray_t* shared_secret)
{
  size_t        len;
  EVP_PKEY_CTX* key_ctx;

  len = 0;
  baInit(shared_secret, 0);
  key_ctx = NULL;

  key_ctx = EVP_PKEY_CTX_new(private_key, NULL);
  if (!key_ctx)
  {
    unsigned long err = ERR_get_error();
    XLOG_ERROR("EVP_PKEY_CTX_new failed:%lu", err);
    return 0;
  }
  
  if (!EVP_PKEY_derive_init(key_ctx))
  {
    XLOG_ERROR("EVP_PKEY_derive_init failed. %s", CRYPTO_Error());
    EVP_PKEY_CTX_free(key_ctx);
    return 0;
  }

  if (!EVP_PKEY_derive_set_peer(key_ctx, peer_key))
  {
    XLOG_ERROR("EVP_PKEY_derive_set_peer failed. %s", CRYPTO_Error());
    EVP_PKEY_CTX_free(key_ctx);
    return 0;
  }

  if (!EVP_PKEY_derive(key_ctx, NULL, &len))
  {
    XLOG_ERROR("EVP_PKEY_derive failed. %s", CRYPTO_Error());
    EVP_PKEY_CTX_free(key_ctx);
    return 0;
  }

  baInit(shared_secret, len);

  if (!EVP_PKEY_derive(key_ctx, shared_secret->data, &len))
  {
    XLOG_ERROR("EVP_PKEY_derive failed. %s", CRYPTO_Error());
    EVP_PKEY_CTX_free(key_ctx);
    return 0;
  }
  else
  {
    shared_secret->length = len;
  }

  EVP_PKEY_CTX_free(key_ctx);
  return 1;
}


void
baInit(
  byteArray_t* v,
  int length)
{
  if (v)
  {
    if (length > 0)
    {
      size_t n = sizeof(byte_t) * length;
      v->data = (byte_t *) malloc(n);
      memset(v->data, 0, n);
      v->length = length;
    }
    else
    {
      v->data = NULL;
      v->length = 0;
    }
  }
}

void
baClear(
  byteArray_t* v)
{
  if (v)
  {
    if (v->data)
      free(v->data);
    v->data = NULL;
    v->length = 0;
  }
}

#if _DEBUG_CRYPTO_
void
baPrint(
  byteArray_t const* v,
  char const* name)
{
  int i;

  printf("\n");
  printf("----- BEGIN %s -----\n", name);
  for (i = 0; i < v->length; ++i)
  {
    if (i > 0 && i % 8 == 0)
      printf("\n");
    printf("0x%02x ", v->data[i]);
  }
  printf("\n----- END %s -----\n", name);
  printf("\n");
}
#endif

int
ECDH_DecryptWiFiSettings(
  cJSON const* server_reply,
  cJSON** wifi_settings)
{
  int     ret;
  cJSON*  iv;
  cJSON*  peer_key;
  cJSON*  settings;
  char*   clear_text;
  int     clear_text_length;

  ret = 0;
  iv = NULL;
  peer_key = NULL;
  settings = NULL;
  clear_text = NULL;
  clear_text_length = 0;

  if (!server_reply)
  {
    XLOG_ERROR("null server_reply");
    return 1;
  }

  if (!wifi_settings)
  {
    XLOG_ERROR("null wifi_settings");
    return 1;
  }

  settings = cJSON_GetObjectItem(server_reply, "wifi-settings");
  if (settings)
  {
    XLOG_INFO("found un-encrypted settings, using those");
    *wifi_settings = cJSON_Duplicate(settings, 1); 
    return 1;
  }

  settings = cJSON_GetObjectItem(server_reply, "secure-wifi-settings");
  if (!settings)
  {
    XLOG_ERROR("failed to find 'secure-wifi-settings' in wifi settings object");
    *wifi_settings = NULL;
    return 0;
  }

  iv = cJSON_GetObjectItem(server_reply, "iv");
  if (!iv)
  {
    XLOG_ERROR("failed to find 'iv' in wifi settings object");
    *wifi_settings = NULL;
    return 0;
  }

  peer_key = cJSON_GetObjectItem(server_reply, "pubKey");
  if (!peer_key)
  {
    XLOG_ERROR("failed to find 'pubKey' in wifi settings object");
    return 0;
  }

  ret = ECDH_Decrypt(kPrivateKeyPath, peer_key->valuestring, iv->valuestring,
    settings->valuestring, &clear_text, &clear_text_length);
  if (!ret)
  {
    XLOG_ERROR("failed to decrypt secure wifi settings from '%s'",
      settings->valuestring);
    return 0;
  }

  if (!clear_text)
  {
    XLOG_ERROR("encryption returned NULL clear text string");
    return 0;
  }

  *wifi_settings = cJSON_Parse(clear_text);
  if (!*wifi_settings)
  {
    XLOG_ERROR("failed to parse secure wifi settings json string '%s'",
      clear_text);
    return 0;
  }

  free(clear_text);
  return 1;
}

char const*
CRYPTO_Error()
{
  static char buff[512];

  unsigned long err = ERR_get_error();
  ERR_error_string_n(err, buff, sizeof(buff));

  return buff;
}

