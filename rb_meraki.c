/*
**
** Copyright (c) 2014, Eneo Tecnologia
** Author: Eugenio Perez <eupm90@gmail.com>
** All rights reserved.
**
** This program is free software; you can redistribute it and/or modify
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Affero General Public License as
** published by the Free Software Foundation, either version 3 of the
** License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Affero General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "rb_meraki.h"
#include "rb_mac.h"
#include "kafka.h"

#include <librd/rdlog.h>
#include <assert.h>
#include <jansson.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <librdkafka/rdkafka.h>

static const char CONFIG_MERAKI_SECRETS_KEY[] = "meraki-secrets";

static const char MERAKI_TYPE_KEY[] = "type";
static const char MERAKI_TYPE_VALUE[] = "meraki";
static const char MERAKI_WIRELESS_STATION_KEY[] = "wireless_station";

static const char MERAKI_SRC_ORIGINAL_KEY[] = "ipv4";
static const char MERAKI_SRC_DESTINATION_KEY[] = "src";

static const char MERAKI_CLIENT_OS_ORIGINAL_KEY[] = "os";
static const char MERAKI_CLIENT_OS_DESTINATION_KEY[] = "client_os";

static const char MERAKI_CLIENT_MAC_VENDOR_ORIGINAL_KEY[] = "manufacturer";
static const char MERAKI_CLIENT_MAC_VENDOR_DESTINATION_KEY[] = "client_mac_vendor";

static const char MERAKI_CLIENT_MAC_ORIGINAL_KEY[] = "clientMac";
static const char MERAKI_CLIENT_MAC_DESTINATION_KEY[] = "client_mac";

static const char MERAKI_TIMESTAMP_ORIGINAL_KEY[] = "seenEpoch";
static const char MERAKI_TIMESTAMP_DESTINATION_KEY[] = "timestamp";

static const char MERAKI_CLIENT_RSSI_NUM_ORIGINAL_KEY[] = "rssi";
static const char MERAKI_CLIENT_RSSI_NUM_DESTINATION_KEY[] = "client_rssi_num";

static const char MERAKI_WIRELESS_ID_ORIGINAL_KEY[] = "ssid";
static const char MERAKI_WIRELESS_ID_DESTINATION_KEY[] = "wireless_id";

static const char MERAKI_LOCATION_KEY[] = "location";
static const char MERAKI_LOCATION_LAT_KEY[] = "lat";
static const char MERAKI_LOCATION_LNG_KEY[] = "lng";
static const char MERAKI_CLIENT_LATLON_DESTINATION_KEY[] = "client_latlong";

/* 
    VALIDATING MERAKI SECRET
*/

int parse_meraki_secrets(struct meraki_database *db,const struct json_t *meraki_secrets,char *err,size_t err_size){
	assert(db);

	json_t *new_db = NULL;

	new_db = json_deep_copy(meraki_secrets);
	if(!new_db){
		snprintf(err,err_size,"Can't create json object (out of memory?)");
		return -1;
	}

	pthread_rwlock_wrlock(&db->rwlock);
	json_t *old_db = db->root;
	db->root = new_db;
	pthread_rwlock_unlock(&db->rwlock);

	if(old_db)
		json_decref(old_db);

	return 0;
}

void meraki_database_done(struct meraki_database *db) {
	json_decref(db->root);
	pthread_rwlock_destroy(&db->rwlock);
}

/* 
    ENRICHMENT
*/

/* Data that should be in all kafka messages */
struct meraki_transversal_data {
	json_t *wireless_station;
	json_t *enrichment;
};

static int rename_key_if_exists(json_t *root,json_t *object,const char *old_key,const char *new_key) {
	if(object && !json_is_null(object)) {
		json_object_set(root,new_key,object);
	}
	return json_object_del(root,old_key) || object ? 0 : -1;
}

static int double_cmp(const double a,const double b) {
	return (a>b) - (a<b);
}

/* transform meraki observation in our suitable keys/values */
static void transform_meraki_observation(json_t *observation,
                           struct meraki_transversal_data *transversal_data) {
	json_error_t jerr;
	
	json_t *src=NULL,*client_os = NULL,*client_mac=NULL,*client_mac_vendor=NULL,
	       *timestamp=NULL,*client_rssi_num=NULL,*wireless_id=NULL;
	double location_lat=0,location_lon=0;

	/*
	if(!transversal_data->enrichment) {
		rdlog(LOG_WARNING,"No enrichment, cannot add %s",MERAKI_WIRELESS_STATION_KEY);
	} else {
		json_t *wireless_station = json_object_get(transversal_data->enrichment,
			                                       MERAKI_WIRELESS_STATION_KEY);
		if(NULL == wireless_station) {
			rdlog(LOG_WARNING,"No %s enrichment", MERAKI_WIRELESS_STATION_KEY);
		} else {
			json_object_set(observation,MERAKI_WIRELESS_STATION_KEY,wireless_station);
		}

	}
	*/

	if(NULL == transversal_data->wireless_station) {
		rdlog(LOG_WARNING,"No %s in meraki message", MERAKI_WIRELESS_STATION_KEY);
	} else {
		json_object_set(observation,MERAKI_WIRELESS_STATION_KEY,
			                       transversal_data->wireless_station);
	}

	const int unpack_rc = json_unpack_ex(observation,&jerr,0,
	 	"{"
	 		"s?o," /* src */
	 		"s?o," /* client_os */
	 		"s:o," /* client_mac */
	 		"s?o," /* client_mac_vendor */
	 		"s:o," /* timestamp */
	 		"s?o," /* client_rssi_num */
	 		"s?o," /* ssid */
	 		"s?{"  /* location */
	 			"s?f,"  /* lat */
	 			"s?f"   /* long */
	 		"}"
	 	"}",
	 	MERAKI_SRC_ORIGINAL_KEY,&src,
	 	MERAKI_CLIENT_OS_ORIGINAL_KEY,&client_os,
		MERAKI_CLIENT_MAC_ORIGINAL_KEY,&client_mac,
		MERAKI_CLIENT_MAC_VENDOR_ORIGINAL_KEY,&client_mac_vendor,
		MERAKI_TIMESTAMP_ORIGINAL_KEY,&timestamp,
		MERAKI_CLIENT_RSSI_NUM_ORIGINAL_KEY,&client_rssi_num,
		MERAKI_WIRELESS_ID_ORIGINAL_KEY,&wireless_id,
		MERAKI_LOCATION_KEY,
			MERAKI_LOCATION_LAT_KEY,&location_lat,
			MERAKI_LOCATION_LNG_KEY,&location_lon
	 );

	if(unpack_rc != 0) {
		rdlog(LOG_ERR,"Can't unpack meraki message: %s",jerr.text);
		return;
	}

	json_t *meraki_type = json_string(MERAKI_TYPE_VALUE);
	json_object_set_new(observation,MERAKI_TYPE_KEY,meraki_type);

	if(src) {
		const char *_src = json_string_value(src);
		if(_src) {
			// src is /XXX.XXX.XXX.XXX ip. Need to chop first /
			json_t *new_src = json_string(&_src[1]);
			json_object_set_new(observation,MERAKI_SRC_DESTINATION_KEY,
				                                              new_src);
		}

		// Delete original form
		json_object_del(observation,MERAKI_SRC_ORIGINAL_KEY);
	}

	if(0!=double_cmp(0,location_lat) && 0!=double_cmp(0,location_lon)) {
		char buf[BUFSIZ];
		snprintf(buf,sizeof(buf),"%.5f,%.5f",location_lat,location_lon);
		json_t *client_latlon = json_string(buf);
		json_object_set_new(observation,MERAKI_CLIENT_LATLON_DESTINATION_KEY,client_latlon);
	}

	json_object_del(observation,MERAKI_LOCATION_KEY);

	rename_key_if_exists(observation,client_os,
	        MERAKI_CLIENT_OS_ORIGINAL_KEY,MERAKI_CLIENT_OS_DESTINATION_KEY);
	rename_key_if_exists(observation,client_mac_vendor,
	        MERAKI_CLIENT_MAC_VENDOR_ORIGINAL_KEY,MERAKI_CLIENT_MAC_VENDOR_DESTINATION_KEY);
	rename_key_if_exists(observation,client_mac,
	        MERAKI_CLIENT_MAC_ORIGINAL_KEY,MERAKI_CLIENT_MAC_DESTINATION_KEY);
	rename_key_if_exists(observation,timestamp,
	        MERAKI_TIMESTAMP_ORIGINAL_KEY,MERAKI_TIMESTAMP_DESTINATION_KEY);
	rename_key_if_exists(observation,client_rssi_num,
	        MERAKI_CLIENT_RSSI_NUM_ORIGINAL_KEY,MERAKI_CLIENT_RSSI_NUM_DESTINATION_KEY);
	rename_key_if_exists(observation,wireless_id,
	        MERAKI_WIRELESS_ID_ORIGINAL_KEY,MERAKI_WIRELESS_ID_DESTINATION_KEY);

}

static void extract_meraki_observation(struct kafka_message_array *msgs,size_t idx,
	json_t *observations,struct meraki_transversal_data *transversal_data) {

	json_t *observation_i = json_array_get(observations,idx);

	if(NULL == observation_i) {
		rdlog(LOG_ERR,"NULL observation %zu. Can't process",idx);
		return;
	}

	transform_meraki_observation(observation_i,transversal_data);

	char *buf = json_dumps(observation_i,JSON_COMPACT|JSON_ENSURE_ASCII);
	/// @TODO Don't use strlen
	save_kafka_msg_in_array(msgs,buf,strlen(buf),NULL);
}

static struct kafka_message_array *extract_meraki_data(json_t *json,struct meraki_database *db) {
	size_t i;
	json_error_t jerr;
	struct meraki_transversal_data meraki_transversal = {NULL,NULL};
	json_t *observations = NULL;

	const char *meraki_secret = NULL;
	const int json_unpack_rc = json_unpack_ex(json,&jerr,0,"{s:s,s:{s:o,s:o}}",
		"secret",&meraki_secret,
		"data",
			"apMac",&meraki_transversal.wireless_station,
			"observations",&observations);

	if(0 != json_unpack_rc) {
		rdlog(LOG_ERR,"Can't decode meraki JSON: \"%s\" in line %d column %d",
			jerr.text,jerr.line,jerr.column);
	}

	if(NULL == meraki_secret) {
		rdlog(LOG_ERR,"Meraki JSON received with no secret. Discarding.");
		return NULL;
	}

	if(NULL == observations) {
		rdlog(LOG_ERR,"Meraki JSON received with no observations. Discarding.");
		return NULL;
	}

	if(!json_is_array(observations)) {
		rdlog(LOG_ERR,"Meraki JSON observations is not an array. Discarding.");
		return NULL;
	}

	pthread_rwlock_rdlock(&db->rwlock);
	json_t *enrichment_tmp = json_object_get(db->root,meraki_secret);
	if(enrichment_tmp)
		meraki_transversal.enrichment = json_deep_copy(enrichment_tmp);
	pthread_rwlock_unlock(&db->rwlock);

	if(NULL == meraki_transversal.enrichment) {
		rdlog(LOG_ERR, "Meraki JSON received with no valid secret (%s). Discarding.",
			meraki_secret);
		return NULL;
	}

	const size_t msgs_size = json_array_size(observations);
	struct kafka_message_array *msgs = new_kafka_message_array(msgs_size);

	for(i=0;i<msgs_size;++i)
		extract_meraki_observation(msgs,i,observations,&meraki_transversal);

	json_decref(meraki_transversal.enrichment);

	return msgs;
}

static struct kafka_message_array *process_meraki_buffer(const char *buffer,size_t bsize,
                                 struct meraki_database *db){
	struct kafka_message_array *notifications = NULL;
	assert(bsize);
	assert(to);

	json_error_t err;
	json_t *json = json_loadb(buffer,bsize,0,&err);
	if(NULL == json){
		rdlog(LOG_ERR,"Error decoding meraki JSON (%s), line %d column %d: %s",
			buffer,err.line,err.column,err.text);
		goto err;
	}

	notifications = extract_meraki_data(json,db);
	if(!notifications || notifications->size == 0) {
		/* Nothing to do here */
		free(notifications);
		notifications = NULL;
		goto err;
	}

err:
	if(json)
		json_decref(json);
	return notifications;
}

void meraki_decode(char *buffer,size_t buf_size,void *_listener_callback_opaque){
	struct meraki_config *meraki_cfg = _listener_callback_opaque;

	struct kafka_message_array *notifications = process_meraki_buffer(buffer,buf_size,&meraki_cfg->database);

	send_array_to_kafka(notifications);
	free(notifications);
	free(buffer);
}