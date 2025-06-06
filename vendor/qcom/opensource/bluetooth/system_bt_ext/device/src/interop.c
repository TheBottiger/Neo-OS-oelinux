/******************************************************************************
 *
 *  Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *  Not a Contribution.
 *  Copyright (C) 2014 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#define LOG_TAG "bt_device_interop"

#include <assert.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <hardware/bluetooth.h>

#include "bt_types.h"
#include "osi/include/config.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/compat.h"
#include "osi/include/list.h"
#include "device/include/interop_database.h"
#include "device/include/interop_config.h"
#include "btcore/include/bdaddr.h"
#include "osi/include/allocator.h"
#include "device/include/interop.h"
#include "btcore/include/module.h"

#if defined(OS_GENERIC)
static const char *INTEROP_FILE_PATH = "interop_database.conf";
#else  // !defined(OS_GENERIC)
static const char *INTEROP_FILE_PATH = "/data/misc/bluedroid/interop_database.conf";
static const char *INTEROP_BASE_FILE_PATH = "/etc/bluetooth/interop_database.conf";
#endif  // defined(OS_GENERIC)

list_t *interop_list = NULL;
bool interop_is_initialized = false;
// protects operations on |interop_list|
pthread_mutex_t interop_list_lock;

// protects operations on |config|
static pthread_mutex_t file_lock;
static config_t *config;

#define CASE_RETURN_STR(const) case const: return #const;
// Macro used to find the total number of feature_types
#define  NO_OF_FEATURES(x)  (sizeof(x) / sizeof((x)[0]))

#define SECTION_MAX_LENGTH    (249)
#define KEY_MAX_LENGTH      (249)
#define VALID_VNDR_PRDT_LEN   (13)
#define VALID_MNFR_STR_LEN    (6)
#define VENDOR_VALUE_SEPARATOR  "-"

#define ADDR_BASED    "Address_Based"
#define NAME_BASED    "Name_Based"
#define MNFR_BASED    "Manufacturer_Based"
#define VNDR_PRDT_BASED   "Vndr_Prdt_Based"

struct config_t {
  list_t *sections;
};

typedef struct {
  char *key;
  char *value;
} interop_entry_t;

typedef struct {
  char *name;
  list_t *entries;
} interop_section_t;

typedef enum {
    INTEROP_BL_TYPE_ADDR = 0,
    INTEROP_BL_TYPE_NAME,
    INTEROP_BL_TYPE_MANUFACTURE,
    INTEROP_BL_TYPE_VNDR_PRDT,

} interop_bl_type;

typedef struct {
    interop_bl_type bl_type;

    union {
        interop_addr_entry_t addr_entry;
        interop_name_entry_t name_entry;
        interop_manufacturer_t mnfr_entry;
        interop_hid_multitouch_t vnr_pdt_entry;
    } entry_type;

} interop_db_entry_t;

// Config realted functions
static void interop_config_cleanup(void);
static void interop_free_entry_(void *data);
static void interop_lazy_init_(void);

//This function is used to initialize the interop list and load the entries from file
static void load_config();
static void interop_database_add_( interop_db_entry_t *db_entry, bool persist);
static bool interop_database_remove_( interop_db_entry_t *entry);
static bool interop_database_match_( interop_db_entry_t *entry, interop_db_entry_t **ret_entry);
static void interop_config_write(UNUSED_ATTR UINT16 event, UNUSED_ATTR char *p_param);


// Interface functions

bool interop_match_addr(const interop_feature_t feature,
                const bt_bdaddr_t *addr)
{
  assert(addr);
  return (interop_database_match_addr(feature, addr));
}

bool interop_match_name(const interop_feature_t feature,
                    const char *name)
{
  assert(name);
  return (interop_database_match_name(feature, name));
}

bool interop_match_manufacturer(const interop_feature_t feature,
                    uint16_t manufacturer)
{
  return (interop_database_match_manufacturer(feature, manufacturer));
}

bool interop_match_vendor_product_ids(const interop_feature_t feature,
    uint16_t vendor_id, uint16_t product_id)
{
  return interop_database_match_vndr_prdt(feature, vendor_id, product_id);
}

void interop_database_add(const uint16_t feature, const bt_bdaddr_t *addr,
                              size_t length)
{
  assert(addr);
  assert(length > 0);
  assert(length < sizeof(bt_bdaddr_t));
  interop_database_add_addr(feature, addr,length);
}

static const char* interop_feature_string_(const interop_feature_t feature)
{
  switch (feature) {
    CASE_RETURN_STR(INTEROP_DISABLE_LE_SECURE_CONNECTIONS)
    CASE_RETURN_STR(INTEROP_AUTO_RETRY_PAIRING)
    CASE_RETURN_STR(INTEROP_DISABLE_ABSOLUTE_VOLUME)
    CASE_RETURN_STR(INTEROP_DISABLE_AUTO_PAIRING)
    CASE_RETURN_STR(INTEROP_KEYBOARD_REQUIRES_FIXED_PIN)
    CASE_RETURN_STR(INTEROP_2MBPS_LINK_ONLY)
    CASE_RETURN_STR(INTEROP_DISABLE_SDP_AFTER_PAIRING)
    CASE_RETURN_STR(INTEROP_DISABLE_AUTH_FOR_HID_POINTING)
    CASE_RETURN_STR(INTEROP_REMOVE_HID_DIG_DESCRIPTOR)
    CASE_RETURN_STR(INTEROP_DISABLE_SNIFF_DURING_SCO)
    CASE_RETURN_STR(INTEROP_INCREASE_AG_CONN_TIMEOUT)
    CASE_RETURN_STR(INTEROP_DISABLE_LE_CONN_PREFERRED_PARAMS)
    CASE_RETURN_STR(INTEROP_ADV_AVRCP_VER_1_3)
    CASE_RETURN_STR(INTEROP_DISABLE_AAC_CODEC)
    CASE_RETURN_STR(INTEROP_DYNAMIC_ROLE_SWITCH)
    CASE_RETURN_STR(INTEROP_DISABLE_ROLE_SWITCH)
    CASE_RETURN_STR(INTEROP_DISABLE_ROLE_SWITCH_POLICY)
    CASE_RETURN_STR(INTEROP_HFP_1_7_BLACKLIST)
    CASE_RETURN_STR(END_OF_INTEROP_LIST)
  }
  return "UNKNOWN";
}

void interop_database_clear()
{
  pthread_mutex_lock(&interop_list_lock);

  if (!interop_is_initialized && interop_list) {
    list_clear(interop_list);
  }
  pthread_mutex_unlock(&interop_list_lock);
}

// Module life-cycle functions

static future_t *interop_init(void)
{
  interop_lazy_init_();
  interop_is_initialized = true;
  return future_new_immediate(FUTURE_SUCCESS);
}

static future_t *interop_clean_up(void)
{
  pthread_mutex_lock(&interop_list_lock);
  list_free(interop_list);
  interop_list = NULL;
  interop_is_initialized = false;
  pthread_mutex_unlock(&interop_list_lock);
  pthread_mutex_destroy(&interop_list_lock);
  interop_config_cleanup();

  return future_new_immediate(FUTURE_SUCCESS);
}

//TODO: Fix this
#ifndef ANDROID
#define EXPORT_SYMBOL   __attribute__((visibility("default")))
#endif

EXPORT_SYMBOL module_t interop_module = {
  .name = INTEROP_MODULE,
  .init = interop_init,
  .start_up = NULL,
  .shut_down = NULL,
  .clean_up = interop_clean_up,
  .dependencies = {NULL},
};

// Local functions

static void interop_free_entry_(void *data)
{
  interop_db_entry_t *entry = (interop_db_entry_t *)data;
  osi_free(entry);
}

static void interop_lazy_init_(void)
{
  pthread_mutex_init(&interop_list_lock, NULL);
  if (interop_list == NULL) {
    interop_list = list_new(interop_free_entry_);
    load_config();
  }
}

// interop config related functions

static int interop_config_init(void)
{
  struct stat sts;
  pthread_mutex_init(&file_lock, NULL);
  pthread_mutex_lock(&file_lock);

  if (!stat(INTEROP_FILE_PATH, &sts) && sts.st_size) {
    if(!(config = config_new(INTEROP_FILE_PATH))) {
      LOG_WARN(LOG_TAG, "%s unable to load config file for : %s",
         __func__, INTEROP_FILE_PATH);
    }
  } else if(!(config = config_new(INTEROP_BASE_FILE_PATH))) {
      LOG_WARN(LOG_TAG, "%s unable to load config file for : %s",
         __func__, INTEROP_BASE_FILE_PATH);
  }

  if(!config  && !(config = config_new_empty()))
    goto error;

  pthread_mutex_unlock(&file_lock);
  return 0;

error:
  config_free(config);
  pthread_mutex_unlock(&file_lock);
  pthread_mutex_destroy(&file_lock);
  config = NULL;
  return -1;
}

static void interop_config_flush(void)
{
  assert(config != NULL);
  interop_config_write(0, NULL);
}

static bool interop_config_remove(const char *section, const char *key)
{
  assert(config != NULL);
  assert(section != NULL);
  assert(key != NULL);

  pthread_mutex_lock(&file_lock);
  bool ret = config_remove_key(config, section, key);
  pthread_mutex_unlock(&file_lock);

  return ret;
}

static bool interop_config_set_str(const char *section, const char *key,
                          const char *value)
{
  assert(config != NULL);
  assert(section != NULL);
  assert(key != NULL);
  assert(value != NULL);

  pthread_mutex_lock(&file_lock);
  config_set_string(config, section, key, value);
  pthread_mutex_unlock(&file_lock);

  return true;
}

static int get_feature(char *section)
{
  interop_feature_t feature;

  for ( feature = BEGINING_OF_INTEROP_LIST;
        feature < END_OF_INTEROP_LIST; feature++ ) {
    if (!strncmp( section, interop_feature_string_(feature),
                      strlen(section)))
      return feature;
  }
  LOG_WARN(LOG_TAG, "Feature is not supported %s", section);
  return -1;
}

static void interop_database_add_( interop_db_entry_t *db_entry,
                          bool persist)
{
  interop_db_entry_t *ret_entry = NULL;

  if (!interop_database_match_(db_entry, &ret_entry)) {
    pthread_mutex_lock(&interop_list_lock);

    if (interop_list) {
      list_append(interop_list, db_entry);
    }
    pthread_mutex_unlock(&interop_list_lock);
  } else {
    //return as the entry is already present
    LOG_DEBUG(LOG_TAG, "Entry is already present in the list");
    return;
  }

  if (!persist) {
    //return if the persist option is not set
    return;
  }

  // add it to the config file as well
  switch (db_entry->bl_type) {
    case INTEROP_BL_TYPE_ADDR:
      {
        interop_feature_t feature =
              db_entry->entry_type.addr_entry.feature;
        bdstr_t bdstr = { '\0' };
        bdaddr_to_string(&db_entry->entry_type.addr_entry.addr,
                    bdstr, sizeof(bdstr));
        bdstr[(db_entry->entry_type.addr_entry.length * 3) - 1] = '\0';
        interop_config_set_str(interop_feature_string_(feature),
                  bdstr, ADDR_BASED);
        interop_config_flush();
        break;
      }
    case INTEROP_BL_TYPE_NAME:
      {
        interop_feature_t feature =
            db_entry->entry_type.name_entry.feature;
        interop_config_set_str(interop_feature_string_(feature),
          db_entry->entry_type.name_entry.name, NAME_BASED);
        interop_config_flush();
        break;
      }
    case INTEROP_BL_TYPE_MANUFACTURE:
      {
        interop_feature_t feature =
            db_entry->entry_type.mnfr_entry.feature;
        char m_facturer[KEY_MAX_LENGTH] = { '\0' };
        snprintf(m_facturer, sizeof(m_facturer), "0x%04x",
            db_entry->entry_type.mnfr_entry.manufacturer);
        interop_config_set_str(interop_feature_string_(feature),
            m_facturer, MNFR_BASED);
        interop_config_flush();
        break;
      }
    case INTEROP_BL_TYPE_VNDR_PRDT:
      {
        interop_feature_t feature =
          db_entry->entry_type.vnr_pdt_entry.feature;
        char m_vendor[KEY_MAX_LENGTH] = { '\0' };
        snprintf(m_vendor, sizeof(m_vendor), "0x%04x-0x%04x",
            db_entry->entry_type.vnr_pdt_entry.vendor_id,
            db_entry->entry_type.vnr_pdt_entry.product_id);
        interop_config_set_str(interop_feature_string_(feature),
            m_vendor, VNDR_PRDT_BASED);
        interop_config_flush();
        break;
      }
  }
}

static bool interop_database_match_( interop_db_entry_t *entry,
                interop_db_entry_t **ret_entry)
{
  assert(entry);
  bool found = false;
  pthread_mutex_lock(&interop_list_lock);
  if (interop_list == NULL || list_length(interop_list) == 0) {
    pthread_mutex_unlock(&interop_list_lock);
    return false;
  }

  const list_node_t *node = list_begin(interop_list);

  while (node != list_end(interop_list)) {
    interop_db_entry_t *db_entry = list_node(node);
    assert(db_entry);

    if (entry->bl_type != db_entry->bl_type) {
      node = list_next(node);
      continue;
    }

    switch (db_entry->bl_type) {
      case INTEROP_BL_TYPE_ADDR:
        {
          interop_addr_entry_t *src = &entry->entry_type.addr_entry;
          interop_addr_entry_t *cur = &db_entry->entry_type.addr_entry;
          if ( (src->feature == cur->feature) &&
              (!memcmp( &src->addr, &cur->addr, cur->length)) ) {
            src->length = cur->length;
            if (ret_entry) {
              *ret_entry = db_entry;
            }
            found = true;
          }
          break;
        }
      case INTEROP_BL_TYPE_NAME:
        {
          interop_name_entry_t *src = &entry->entry_type.name_entry;
          interop_name_entry_t *cur = &db_entry->entry_type.name_entry;

          if ((src->feature == cur->feature) &&
              (strlen(src->name) == cur->length) &&
              (strncasecmp(src->name, cur->name, cur->length) == 0)) {
            if (ret_entry) {
              *ret_entry = db_entry;
            }
            found = true;
          }
          break;
        }
      case INTEROP_BL_TYPE_MANUFACTURE:
        {
          interop_manufacturer_t *src = &entry->entry_type.mnfr_entry;
          interop_manufacturer_t *cur = &db_entry->entry_type.mnfr_entry;

          if (src->feature  == cur->feature &&
              src->manufacturer  == cur->manufacturer) {
            if (ret_entry) {
              *ret_entry = db_entry;
            }
            found = true;
          }
          break;
        }
      case INTEROP_BL_TYPE_VNDR_PRDT:
        {
          interop_hid_multitouch_t *src = &entry->entry_type.vnr_pdt_entry;
          interop_hid_multitouch_t *cur = &db_entry->entry_type.vnr_pdt_entry;

          if ( (src->feature  == cur->feature) &&
              (src->vendor_id == cur->vendor_id) &&
              (src->product_id == cur->product_id) ) {
            if (ret_entry) {
              *ret_entry = db_entry;
            }
            found = true;
          }
          break;
        }
    }

    if (found) {
      break;
    }
    node = list_next(node);
  }
  pthread_mutex_unlock(&interop_list_lock);
  return found;
}

static bool interop_database_remove_( interop_db_entry_t *entry)
{
  bool status = true;
  interop_db_entry_t *ret_entry = NULL;

  if (!interop_database_match_(entry, &ret_entry)) {
    LOG_ERROR(LOG_TAG, "%s Entry not found in the list", __func__);
    return false;
  }

  // first remove it from linked list
  pthread_mutex_lock(&interop_list_lock);
  list_remove(interop_list, (void*)ret_entry);
  pthread_mutex_unlock(&interop_list_lock);

  // remove it from the file
  switch (entry->bl_type) {
    case INTEROP_BL_TYPE_ADDR:
      {
        interop_addr_entry_t *src = &entry->entry_type.addr_entry;

        interop_feature_t feature = src->feature;
        bdstr_t bdstr = { '\0' };
        bdaddr_to_string(&src->addr, bdstr, sizeof(bdstr));
        bdstr[(src->length * 3) - 1] = '\0';
        interop_config_remove(interop_feature_string_(feature), bdstr);
        interop_config_flush();
        break;
      }
    case INTEROP_BL_TYPE_NAME:
      {
        interop_name_entry_t *src = &entry->entry_type.name_entry;
        interop_feature_t feature = src->feature;
        interop_config_remove(interop_feature_string_(feature),
            src->name);
        interop_config_flush();
        break;
      }
    case INTEROP_BL_TYPE_MANUFACTURE:
      {
        interop_manufacturer_t *src = &entry->entry_type.mnfr_entry;
        interop_feature_t feature = src->feature;
        char m_facturer[KEY_MAX_LENGTH] = { '\0' };
        snprintf(m_facturer, sizeof(m_facturer), "0x%04x",
            src->manufacturer);
        interop_config_remove(interop_feature_string_(feature),
            m_facturer);
        interop_config_flush();
        break;
      }
    case INTEROP_BL_TYPE_VNDR_PRDT:
      {
        interop_hid_multitouch_t *src = &entry->entry_type.vnr_pdt_entry;

        interop_feature_t feature = src->feature;
        char m_vendor[KEY_MAX_LENGTH] = { '\0' };
        snprintf(m_vendor, sizeof(m_vendor), "0x%04x-0x%04x",
            src->vendor_id, src->product_id);
        interop_config_remove(interop_feature_string_(feature),
            m_vendor);
        interop_config_flush();
        break;
      }
    default:
    status = false;
  }

  return status;
}

static char *trim(char *str)
{
  while (isspace(*str))
    ++str;

  if (!*str)
    return str;

  char *end_str = str + strlen(str) - 1;
  while (end_str > str && isspace(*end_str))
    --end_str;

  end_str[1] = '\0';
  return str;
}

static bool get_vendor_product_id(char *vendorstr, uint16_t *vendor,
                uint16_t *product)
{
  char *token;
  char *saveptr;
  char *e;
  bool ret_value = false;

  if ( (token = strtok_r(vendorstr, VENDOR_VALUE_SEPARATOR,
                  &saveptr)) != NULL) {
    trim(token);
    errno = 0;
    *vendor = (uint16_t)strtoul(token, &e, 16);
    if ( *e || errno == EINVAL || errno == ERANGE )
      return false;
  }
  if ( (token = strtok_r(NULL, VENDOR_VALUE_SEPARATOR, &saveptr)) != NULL) {
    trim(token);
    errno = 0;
    *product = (uint16_t)strtoul(token, &e, 16);
    if ( (e != NULL) || errno != EINVAL || errno != ERANGE )
      ret_value = true;
  }
  return ret_value;
}

bool load_to_database(int feature, char *key, char *value)
{
  if ( !strncasecmp( value, ADDR_BASED, strlen(ADDR_BASED)) ) {
    bdstr_t bdstr = { '\0' };
    bt_bdaddr_t addr;
    int len = 0;
    char append_str[] = ":00";

    len = (strlen(key) + 1) / 3;
    if ( len < 3 && len > 6 ) {
      LOG_WARN(LOG_TAG, "%s Ignoring as invalid entry for Address %s",
      __func__, key);
      return false;
    }

    snprintf(bdstr, sizeof(bdstr), "%s", key);
    for ( int i = 6;  i > len; i-- )
      strlcat(bdstr, append_str, sizeof(bdstr_t));

    if (!string_is_bdaddr(bdstr)) {
      LOG_WARN(LOG_TAG,
      "%s Bluetooth Address %s is invalid, not added to interop list",
      __func__, key);
      return false;
    }
    string_to_bdaddr(bdstr, &addr);

    interop_db_entry_t *entry = osi_calloc(sizeof(interop_db_entry_t));
    entry->bl_type = INTEROP_BL_TYPE_ADDR;
    bdaddr_copy(&entry->entry_type.addr_entry.addr, &addr);
    entry->entry_type.addr_entry.feature = feature;
    entry->entry_type.addr_entry.length = len;
    interop_database_add_(entry, false);

  } else if ( !strncasecmp( value, NAME_BASED, strlen(NAME_BASED)) ) {
    if ( strlen(key) > KEY_MAX_LENGTH - 1) {
      LOG_WARN(LOG_TAG, " ignoring %s due to invalid length", key);
      return false;
    }
    interop_db_entry_t *entry = osi_calloc(sizeof(interop_db_entry_t));
    entry->bl_type = INTEROP_BL_TYPE_NAME;
    memcpy(&entry->entry_type.name_entry.name, key, strlen(key));
    entry->entry_type.name_entry.feature = feature;
    entry->entry_type.name_entry.length = strlen(key);
    interop_database_add_(entry, false);

  } else if ( !strncasecmp( value, MNFR_BASED, strlen(MNFR_BASED))) {

    uint16_t manufacturer;
    char *e;

    if ( strlen(key) != VALID_MNFR_STR_LEN ) {
      LOG_WARN(LOG_TAG,
      " ignoring %s due to invalid Manufacturer id in config file", key);
      return false;
    }

    manufacturer = (uint16_t)strtoul(key, &e, 16);
    errno = 0;
    if( *e || errno == EINVAL || errno == ERANGE )
       return false;
    interop_db_entry_t *entry = osi_calloc(sizeof(interop_db_entry_t));
    entry->bl_type = INTEROP_BL_TYPE_MANUFACTURE;
    entry->entry_type.mnfr_entry.feature = feature;
    entry->entry_type.mnfr_entry.manufacturer = manufacturer;
    interop_database_add_(entry, false);

  } else if ( !strncasecmp( value, VNDR_PRDT_BASED, strlen(VNDR_PRDT_BASED))) {

    uint16_t vendor_id;
    uint16_t product_id;
    char tmp_key[KEY_MAX_LENGTH] = { '\0' };

    if ( strlen(key) != VALID_VNDR_PRDT_LEN ) {
      LOG_WARN(LOG_TAG,
      " ignoring %s due to invalid vendor/product id in config file",key);
      return false;
    }

    strlcpy(tmp_key, key, KEY_MAX_LENGTH);
    if (!get_vendor_product_id(tmp_key, &vendor_id, &product_id)) {
      LOG_WARN(LOG_TAG, " Error in parsing vendor/product id %s", key);
      return false;
    }

    interop_db_entry_t *entry = osi_calloc(sizeof(interop_db_entry_t));
    entry->bl_type = INTEROP_BL_TYPE_VNDR_PRDT;
    entry->entry_type.vnr_pdt_entry.feature = feature;
    entry->entry_type.vnr_pdt_entry.vendor_id = vendor_id;
    entry->entry_type.vnr_pdt_entry.product_id = product_id;
    interop_database_add_(entry, false);
  }
  LOG_WARN(LOG_TAG, " feature:: %d, key :: %s, value :: %s",
                    feature, key, value);
  return true;
}

static void load_config()
{
  if ( interop_config_init() != -1) {
    pthread_mutex_lock(&file_lock);
    for (const list_node_t *node = list_begin(config->sections);
       node != list_end(config->sections); node = list_next(node)) {
      int feature = -1;
      interop_section_t *sec = list_node(node);
      if ( (feature = get_feature(sec->name)) != -1 ) {
        for (const list_node_t *node_entry = list_begin(sec->entries);
           node_entry != list_end(sec->entries);
           node_entry = list_next(node_entry)) {
          interop_entry_t *entry = list_node(node_entry);
          load_to_database(feature, entry->key, entry->value);
        }
      }
      else {
        LOG_ERROR(LOG_TAG, " Invalid Section, ignoring the section %s",
                            sec->name);
      }
    }
    pthread_mutex_unlock(&file_lock);
  }
  else {
    LOG_ERROR(LOG_TAG, "Error in initializing interop config file");
  }
}

static void interop_config_write(UNUSED_ATTR UINT16 event, UNUSED_ATTR char *p_param)
{
  assert(config != NULL);

  pthread_mutex_lock(&file_lock);
  config_save(config, INTEROP_FILE_PATH);
  // sync the file as well
  int fd = open (INTEROP_FILE_PATH, O_WRONLY | O_APPEND, 0660);
  if (fd != -1) {
    fsync (fd);
    close (fd);
  }
  pthread_mutex_unlock(&file_lock);
}

static void interop_config_cleanup(void)
{
  interop_config_flush();

  pthread_mutex_lock(&file_lock);
  config_free(config);
  config = NULL;
  pthread_mutex_unlock(&file_lock);
  pthread_mutex_destroy(&file_lock);
}

void interop_database_add_addr(const uint16_t feature, const bt_bdaddr_t *addr,
                                size_t length)
{
  assert(addr);
  assert(length);

  interop_db_entry_t *entry = osi_calloc(sizeof(interop_db_entry_t));
  entry->bl_type = INTEROP_BL_TYPE_ADDR;
  memcpy(&entry->entry_type.addr_entry.addr, addr, length);
  entry->entry_type.addr_entry.feature = feature;
  entry->entry_type.addr_entry.length = length;
  interop_database_add_(entry, true);
}

void interop_database_add_name(const uint16_t feature, const char *name)
{
  assert(name);
  interop_db_entry_t *entry = osi_calloc(sizeof(interop_db_entry_t));
  entry->bl_type = INTEROP_BL_TYPE_NAME;
  memcpy(&entry->entry_type.name_entry.name, name, strlen(name));
  entry->entry_type.name_entry.feature = feature;
  entry->entry_type.name_entry.length = strlen(name);
  interop_database_add_(entry, true);
}

void interop_database_add_manufacturer(const interop_feature_t feature,
                    uint16_t manufacturer)
{
  assert(manufacturer);

  interop_db_entry_t *entry = osi_calloc(sizeof(interop_db_entry_t));
  entry->bl_type = INTEROP_BL_TYPE_MANUFACTURE;
  entry->entry_type.mnfr_entry.feature = feature;
  entry->entry_type.mnfr_entry.manufacturer = manufacturer;
  interop_database_add_(entry, true);
}

void interop_database_add_vndr_prdt(const interop_feature_t feature,
                  uint16_t vendor_id, uint16_t product_id)
{
  assert(vendor_id);
  assert(product_id);

  interop_db_entry_t *entry = osi_calloc(sizeof(interop_db_entry_t));
  entry->bl_type = INTEROP_BL_TYPE_VNDR_PRDT;
  entry->entry_type.vnr_pdt_entry.feature = feature;
  entry->entry_type.vnr_pdt_entry.vendor_id = vendor_id;
  entry->entry_type.vnr_pdt_entry.product_id = product_id;
  interop_database_add_(entry, true);
}


bool interop_database_match_manufacturer(const interop_feature_t feature,
                      uint16_t manufacturer)
{
  assert(manufacturer);

  interop_db_entry_t entry;
  interop_db_entry_t *ret_entry = NULL;

  entry.bl_type = INTEROP_BL_TYPE_MANUFACTURE;
  entry.entry_type.mnfr_entry.feature = feature;
  entry.entry_type.mnfr_entry.manufacturer = manufacturer;

  if (interop_database_match_(&entry, &ret_entry)) {
    LOG_WARN(LOG_TAG, "%s() Device with manufacturer id: %d is a match for interop "
      "workaround %s", __func__, manufacturer, interop_feature_string_(feature));
    return true;
  }

  return false;
}

bool interop_database_match_name( const interop_feature_t feature, const char *name)
{
  assert(name);

  interop_db_entry_t entry;
  interop_db_entry_t *ret_entry = NULL;

  entry.bl_type = INTEROP_BL_TYPE_NAME;
  strlcpy(entry.entry_type.name_entry.name, name, KEY_MAX_LENGTH);
  entry.entry_type.name_entry.feature = feature;
  entry.entry_type.name_entry.length = strlen(entry.entry_type.name_entry.name);

  if (interop_database_match_(&entry, &ret_entry)) {
    LOG_WARN(LOG_TAG,
    "%s() Device with name: %s is a match for interop workaround %s", __func__,
      name, interop_feature_string_(feature));
    return true;
  }

  return false;
}

bool interop_database_match_addr(const interop_feature_t feature, const bt_bdaddr_t *addr)
{
  assert(addr);

  interop_db_entry_t entry;
  interop_db_entry_t *ret_entry = NULL;

  entry.bl_type = INTEROP_BL_TYPE_ADDR;
  memcpy(&entry.entry_type.addr_entry.addr, addr, sizeof(bt_bdaddr_t));
  entry.entry_type.addr_entry.feature = feature;
  entry.entry_type.addr_entry.length = sizeof(bt_bdaddr_t);

  if (interop_database_match_(&entry, &ret_entry)) {
    bdstr_t bdstr = { '\0' };
    LOG_WARN(LOG_TAG, "%s() Device %s is a match for interop workaround %s.",
      __func__, bdaddr_to_string(addr, bdstr, sizeof(bdstr)),
      interop_feature_string_(feature));
    return true;
  }

  return false;
}

bool interop_database_match_vndr_prdt(const interop_feature_t feature,
                   uint16_t vendor_id, uint16_t product_id)
{
  assert(vendor_id);
  assert(product_id);

  interop_db_entry_t entry;
  interop_db_entry_t *ret_entry = NULL;

  entry.bl_type = INTEROP_BL_TYPE_VNDR_PRDT;

  entry.entry_type.vnr_pdt_entry.feature = feature;
  entry.entry_type.vnr_pdt_entry.vendor_id = vendor_id;
  entry.entry_type.vnr_pdt_entry.product_id = product_id;
  if (interop_database_match_(&entry, &ret_entry)) {
    LOG_WARN(LOG_TAG,
      "%s() Device with vendor_id: %d product_id: %d is a match for "
      "interop workaround %s", __func__, vendor_id, product_id,
      interop_feature_string_(feature));
    return true;
  }

  return false;
}

bool interop_database_remove_name( const interop_feature_t feature, const char *name)
{
  assert(name);

  interop_db_entry_t entry;

  entry.bl_type = INTEROP_BL_TYPE_NAME;
  strlcpy(entry.entry_type.name_entry.name, name, KEY_MAX_LENGTH);
  entry.entry_type.name_entry.feature = feature;
  entry.entry_type.name_entry.length = strlen(entry.entry_type.name_entry.name);
  if (interop_database_remove_(&entry)) {
    LOG_WARN(LOG_TAG,
      "%s() Device with name: %s is removed from interop workaround %s",
      __func__, name, interop_feature_string_(feature));
    return true;
  }

  return false;
}

bool interop_database_remove_manufacturer( const interop_feature_t feature,
                          uint16_t manufacturer)
{
  assert(manufacturer);

  interop_db_entry_t entry;

  entry.bl_type = INTEROP_BL_TYPE_MANUFACTURE;
  entry.entry_type.mnfr_entry.feature = feature;
  entry.entry_type.mnfr_entry.manufacturer = manufacturer;
  if (interop_database_remove_(&entry)) {
    LOG_WARN(LOG_TAG,
      "%s() Device with manufacturer id: %d is removed from interop "
      "workaround %s", __func__, manufacturer,
      interop_feature_string_(feature));
    return true;
  }

  return false;
}


bool interop_database_remove_addr(const interop_feature_t feature,
                    const bt_bdaddr_t *addr)
{
  assert(addr);

  interop_db_entry_t entry;

  entry.bl_type = INTEROP_BL_TYPE_ADDR;
  memcpy(&entry.entry_type.addr_entry.addr, addr, sizeof(bt_bdaddr_t));
  entry.entry_type.addr_entry.feature = feature;
  entry.entry_type.addr_entry.length = sizeof(bt_bdaddr_t);
  if (interop_database_remove_(&entry)) {
    bdstr_t bdstr = { '\0' };
    LOG_WARN(LOG_TAG,
      "%s() Device %s is a removed from interop workaround %s.",
      __func__, bdaddr_to_string(addr, bdstr, sizeof(bdstr)),
      interop_feature_string_(feature));
    return true;
  }

  return false;
}

bool interop_database_remove_vndr_prdt(const interop_feature_t feature,
          uint16_t vendor_id, uint16_t product_id)
{
  assert(vendor_id);
  assert(product_id);

  interop_db_entry_t entry;

  entry.bl_type = INTEROP_BL_TYPE_VNDR_PRDT;

  entry.entry_type.vnr_pdt_entry.feature = feature;
  entry.entry_type.vnr_pdt_entry.vendor_id = vendor_id;
  entry.entry_type.vnr_pdt_entry.product_id = product_id;

  if (interop_database_remove_(&entry)) {
    LOG_WARN(LOG_TAG,
      "%s() Device with vendor_id: %d product_id: %d is removed from"
      "interop workaround %s", __func__, vendor_id, product_id,
      interop_feature_string_(feature));
    return true;
  }
  return false;
}
