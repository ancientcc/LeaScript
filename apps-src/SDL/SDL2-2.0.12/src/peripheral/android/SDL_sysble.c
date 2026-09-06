#include "../SDL_ble_c.h"
#include "../../core/android/SDL_android.h"
#include "../../events/SDL_events_c.h"
#include "SDL_mutex.h"
#include "SDL_log.h"
#include <errno.h>

static jobject adapter = NULL;
static SDL_bool center_initialized = SDL_FALSE;
static const char* UUID_CLIENT_CHARACTERISTIC_CONFIG = "00002902-0000-1000-8000-00805f9b34fb";

#define MAX_SERVICE_UUID_LEN	36 // 00005356-0000-1000-8000-00805f9b34fb
typedef struct {
	uint32_t type;
	uint8_t address[SDL_BLE_MAC_ADDR_BYTES];
	char service_uuid[MAX_SERVICE_UUID_LEN + 1];
	char* name;
	int device_type;
	int rssi;
	uint8_t* manufacturerdata;
	int manufacturerdata_len;
} DidDiscoverPeripheral;

typedef struct {
	uint32_t type;
	uint8_t address[SDL_BLE_MAC_ADDR_BYTES];
	SDL_bool connected;
} DidConnectionStateChange;

typedef struct {
	uint32_t type;
	jobject gatt;
	SDL_bool success;
} DidServicesDiscovered;

typedef struct {
	uint32_t type;
	jobject chara;
	jbyteArray buffer;
	SDL_bool notify;
} DidCharacteristicRead;

typedef struct {
	uint32_t type;
	jobject chara;
	SDL_bool success;
} DidCharacteristicWrite;

typedef struct {
	uint32_t type;
	jobject descriptor;
} DidDescriptorWrite;

typedef struct {
	uint32_t type;
	uint8_t address[SDL_BLE_MAC_ADDR_BYTES];
	SDL_bool connected;
} DidPConnectionStateChange;

typedef struct {
	uint32_t type;
	char* chara;
	jbyteArray buffer;
	SDL_bool notify;
} DidPCharacteristicRead;

typedef struct {
	uint32_t type;
	char* chara;
	int status;
} DidPNotificationSent;

typedef union {
	uint32_t type;
	DidDiscoverPeripheral DiscoverPeripheral;
	DidConnectionStateChange ConnectionStateChange;
	DidServicesDiscovered ServicesDiscovered;
	DidCharacteristicRead CharacteristicRead;
	DidCharacteristicWrite CharacteristicWrite;
	DidDescriptorWrite DescriptorWrite;

	DidPConnectionStateChange PConnectionStateChange;
	DidPCharacteristicRead PCharacteristicRead;
	DidPNotificationSent PNotificationSent;
} BleDid;

#define BLEDID_COUNT	24
static BleDid BleDids[BLEDID_COUNT];
static int bledids_wt = 0;
static int bledids_rd = -1;
static SDL_mutex* bledid_lock = NULL;

static void nativeDiscoverPeripheral(JNIEnv* env, uint8_t* address, char* service_uuid, char* name, int device_type, int rssi, uint8_t* manufacturerdata, int manufacturerdata_len);
static void nativeConnectionStateChange(JNIEnv* env, uint8_t* address, SDL_bool connected);
static void nativeServicesDiscovered(JNIEnv* env, jobject gatt, SDL_bool success);
static void nativeCharacteristicRead(JNIEnv* env, jobject chara, jbyteArray buffer, jboolean notify);
static void nativeCharacteristicWrite(JNIEnv* env, jobject chara, jboolean success);
static void nativeDescriptorWrite(JNIEnv* env, jobject descriptor);
// use as peripheral
void nativePConnectionStateChange(JNIEnv* env, uint8_t* address, SDL_bool connected);
void nativePCharacteristicRead(JNIEnv* env, char* chara, jbyteArray buffer, jboolean notify);
void nativePNotificationSent(JNIEnv* env, char* chara, int status);

void push_bledid(BleDid* did);
void Android_PumpBleDid(SDL_bool foreground);

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativeDiscoverPeripheral(
    JNIEnv* env, jclass jcls,
    jstring jaddress, jstring jservice_uuid, jstring jname, jint rssi, jint device_type, jbyteArray manufacturerdata);

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativeConnectionStateChange(
    JNIEnv* env, jclass jcls,
    jstring jaddress, jboolean connected);

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativeServicesDiscovered(
    JNIEnv* env, jclass jcls,
    jobject gatt, jboolean success);

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativeCharacteristicRead(
    JNIEnv* env, jclass jcls,
    jobject chara, jbyteArray buffer, jboolean notify);

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativeCharacteristicWrite(
    JNIEnv* env, jclass jcls,
    jobject chara, jboolean success);

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativeDescriptorWrite(
    JNIEnv* env, jclass jcls,
    jobject descriptor);

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativePConnectionStateChange(
    JNIEnv* env, jclass jcls,
    jstring address, jboolean connected);

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativePCharacteristicRead(
    JNIEnv* env, jclass jcls,
    jstring chara, jbyteArray buffer, jboolean notify);

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativePNotificationSent(
    JNIEnv* env, jclass jcls,
    jstring chara, jint status);

void push_bledid(BleDid* did)
{
	// SDL_Log("#%i, %i, write, type: %i, <#%i>", bledids_wt, SDL_GetTicks(), did->type, times ++);

	if (bledids_rd == -1) {
		bledids_rd = bledids_wt;
	}

	bledids_wt ++;
	if (bledids_wt == BLEDID_COUNT) {
		bledids_wt = 0;
	}

	// max cache is BLEDID_COUNT - 1.
	if (bledids_rd == bledids_wt) {
		// if read at this, to avoid race to loop, move read
		SDL_Log("bledids_rd(%i) == bledids_wt, will increase.", bledids_rd);
		bledids_rd ++;
		if (bledids_rd == BLEDID_COUNT) {
			bledids_rd = 0;
		}
	}
}

// They don't call by Java VM, require to release all local references by themself.
// I cannot make sure not leak release one local references, use PushLocalFrame/PopLocalFrame.
#define LocalReference_Init(env)	\
	const int capacity = 16;	\
	(*(env))->PushLocalFrame(env, capacity)

#define LocalReference_Cleanup(env)	\
	(*(env))->PopLocalFrame(env, NULL)

void Android_PumpBleDid(SDL_bool foreground)
{
	// in order to avoid Lock/Unlock, place condition outer.
	if (bledids_rd == -1) {
		if (!foreground) {
			SDL_Log("Android_PumpBleDid, [1] bledids_rd == -1, %s", foreground? "foreground": "background");
		}
		return;
	}

	SDL_LockMutex(bledid_lock);
	// below condition should place again after LockMutex.
	// other thread may modify beldids_rd when this thread LockMutex.
	if (bledids_rd == -1) {
		SDL_Log("Android_PumpBleDid, [2] bledids_rd == -1, %s", foreground? "foreground": "background");
		SDL_UnlockMutex(bledid_lock);
		return;
	}

	JNIEnv* env = Android_JNI_GetEnv();
	LocalReference_Init(env);

	for ( ; bledids_rd != bledids_wt; ) {
		BleDid* did = &BleDids[bledids_rd];
		// SDL_Log("#%i, %i, read, type: %i, %s", bledids_rd, SDL_GetTicks(), did->type, foreground? "foreground": "background");

		if (did->type == did_discoverperipheral) {
			nativeDiscoverPeripheral(env, did->DiscoverPeripheral.address, did->DiscoverPeripheral.service_uuid, did->DiscoverPeripheral.name, did->DiscoverPeripheral.device_type, did->DiscoverPeripheral.rssi, did->DiscoverPeripheral.manufacturerdata, did->DiscoverPeripheral.manufacturerdata_len);

		} else if (did->type == did_connectionstatechange) {
			nativeConnectionStateChange(env, did->ConnectionStateChange.address, did->ConnectionStateChange.connected);

		} else if (did->type == did_servicesdiscovered) {
			nativeServicesDiscovered(env, did->ServicesDiscovered.gatt, did->ServicesDiscovered.success);

		} else if (did->type == did_characteristicread) {
			nativeCharacteristicRead(env, did->CharacteristicRead.chara, did->CharacteristicRead.buffer, did->CharacteristicRead.notify);

		} else if (did->type == did_characteristicwrite) {
			nativeCharacteristicWrite(env, did->CharacteristicWrite.chara, did->CharacteristicWrite.success);

		} else if (did->type == did_descriptorwrite) {
			nativeDescriptorWrite(env, did->DescriptorWrite.descriptor);

		} else if (did->type == did_pconnectionstatechange) {
			nativePConnectionStateChange(env, did->PConnectionStateChange.address, did->PConnectionStateChange.connected);

		} else if (did->type == did_pcharacteristicread) {
			nativePCharacteristicRead(env, did->PCharacteristicRead.chara, did->PCharacteristicRead.buffer, did->PCharacteristicRead.notify);

		} else if (did->type == did_pnotificationsent) {
			nativePNotificationSent(env, did->PNotificationSent.chara, did->PNotificationSent.status);

		} 

		// if read at this, to avoid race to loop, move read
		bledids_rd ++;
		if (bledids_rd == BLEDID_COUNT) {
			bledids_rd = 0;
		}
	}
	LocalReference_Cleanup(env);

	// no readable data
	bledids_rd = -1;
	SDL_UnlockMutex(bledid_lock);
}

static int discover_peripheral_blance = 0;
void nativeDiscoverPeripheral(JNIEnv* env, uint8_t* address, char* service_uuid, char* name, int device_type, int rssi, uint8_t* manufacturerdata, int manufacturerdata_len)
{
	discover_peripheral_blance --;
	SDL_Log("[SDL_sysble.c]nativeDiscoverPeripheral--- discover_peripheral_blance: %i service_uuid: %s name: %s, device_type: %i, manufacturerdata: (%p, %i), valid_ble_peripherals: %i", discover_peripheral_blance, service_uuid, name, device_type, manufacturerdata, manufacturerdata_len, valid_ble_peripherals);

	if (name == NULL || name[0] == '\0' || !mac_addr_valid(address)) {
		// invalid mac addr. do nothing.
		if (name != NULL) {
			SDL_free(name);
		}
		SDL_free(manufacturerdata);
		SDL_Log("---[SDL_sysble.c]nativeDiscoverPeripheral X, name or address is invalid. do nothing.");
		return;
	}

	SDL_BlePeripheral* peripheral = discover_peripheral_uh_macaddr(address, name);
	if (!mac_addr_valid(peripheral->mac_addr)) {
		// On android, Since mac_addr is unique, must use it as SDL_BlePeripheral's key. fill it.
		// SDL_Log("[SDL_sysble.c]nativeDiscoverPeripheral, 5, (!mac_addr_valid(peripheral->mac_addr), peripheral->mac_addr: %p, address: %p", peripheral->mac_addr, address);
		SDL_memcpy(peripheral->mac_addr, address, SDL_BLE_MAC_ADDR_BYTES);
	}
	if (name != NULL) {
		SDL_free(name);
		name = NULL;
	}

	// service uuid16 --> uuid
	if (peripheral->uuid != NULL) {
		SDL_free(peripheral->uuid);
		peripheral->uuid = NULL;
	}
	if (service_uuid != NULL && service_uuid[0] != '\0') {
		int len = (int)SDL_strlen(service_uuid);
		peripheral->uuid = (char*)SDL_malloc(len + 1);
		memcpy(peripheral->uuid, service_uuid, len);
		peripheral->uuid[len] = '\0';
	}

	// (manufacturer_data, manufacturer_data_len)
	if (peripheral->manufacturer_data_len != 0) {
		// free original manufacturer data if exist.
		peripheral->manufacturer_data_len = 0;
		SDL_free(peripheral->manufacturer_data);
		peripheral->manufacturer_data = NULL;
	}
	if (manufacturerdata_len != 0) {
		peripheral->manufacturer_data_len = manufacturerdata_len;
		// use param's manufacturerdata directly.
		peripheral->manufacturer_data = manufacturerdata;
	}

	// SDL_Log("[SDL_sysble.c]nativeDiscoverPeripheral, 6, manufacturer_data(len: %i, ptr:0x%p)", peripheral->manufacturer_data_len, peripheral->manufacturer_data);
	discover_peripheral_bh(peripheral, device_type, rssi);

	SDL_Log("---[SDL_sysble.c]nativeDiscoverPeripheral, X");
}

#define PUSH_BLEDID(EVALUATE)	\
	SDL_LockMutex(bledid_lock);	\
	BleDid* did = &BleDids[bledids_wt];	\
	SDL_memset(did, 0, sizeof(BleDid)); \
	EVALUATE	\
	push_bledid(did);	\
	SDL_UnlockMutex(bledid_lock);

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativeDiscoverPeripheral(
    JNIEnv* env, jclass jcls,
    jstring jaddress, jstring jservice_uuid, jstring jname, jint device_type, jint rssi, jbyteArray manufacturerdata)
{
	discover_peripheral_blance ++;
	// SDL_Log("[SDL_sysble.c]Java_org_librose_SDLBle_nativeDiscoverPeripheral--- discover_peripheral_blance: %i, manufacturerdata: %p", discover_peripheral_blance, manufacturerdata);

	const char* address = (*env)->GetStringUTFChars(env, jaddress, NULL);
	uint8_t uc6[SDL_BLE_MAC_ADDR_BYTES];
	mac_addr_str_2_uc6(address, uc6, ':');
	(*env)->ReleaseStringUTFChars(env, jaddress, address);

	const char* service_uuid = NULL;
	if (jservice_uuid != NULL) {
		service_uuid = (*env)->GetStringUTFChars(env, jservice_uuid, NULL);
	}

	const char* name = NULL;
	if (jname != NULL) {
		name = (*env)->GetStringUTFChars(env, jname, NULL);
	}

	uint8_t* data = NULL;
	int data_len = 0;
	if (manufacturerdata != NULL) {
		jbyte* elements = (*env)->GetByteArrayElements(env, manufacturerdata, NULL);
		data_len = (*env)->GetArrayLength(env, (jbyteArray)manufacturerdata);
		data = SDL_malloc(data_len);
		SDL_memcpy(data, elements, data_len);
		(*env)->ReleaseByteArrayElements(env, manufacturerdata, elements, JNI_ABORT);
	}

	PUSH_BLEDID(
		did->type = did_discoverperipheral;
		if (service_uuid != NULL && SDL_strlen(service_uuid) <= MAX_SERVICE_UUID_LEN) {
			SDL_memcpy(did->DiscoverPeripheral.service_uuid, service_uuid, SDL_strlen(service_uuid));
		}
		SDL_memcpy(did->DiscoverPeripheral.address, uc6, SDL_BLE_MAC_ADDR_BYTES);
		if (name != NULL && name[0] != '\0') {
			did->DiscoverPeripheral.name = SDL_strdup(name);
		}
		did->DiscoverPeripheral.device_type = device_type;
		did->DiscoverPeripheral.rssi = rssi;
		did->DiscoverPeripheral.manufacturerdata = data;
		did->DiscoverPeripheral.manufacturerdata_len = data_len;
		);
	if (jservice_uuid != NULL) {
		(*env)->ReleaseStringUTFChars(env, jservice_uuid, service_uuid);
	}
	if (jname != NULL) {
		(*env)->ReleaseStringUTFChars(env, jname, name);
	}
}

void nativeConnectionStateChange(JNIEnv* env, uint8_t* address, SDL_bool connected)
{
	SDL_Log("{blebug}nativeConnectionStateChange---connected: %s", connected? "true": "false");

	SDL_BlePeripheral* peripheral = find_peripheral_from_macaddr(address);
	if (connected) {
		connect_peripheral_bh(peripheral, 0);
	} else {
		// On android, think all disconnect to except disconnect.
		// SDL_Log("nativeConnectionStateChange, disconnect, will call disconnect_peripheral_bh");
		disconnect_peripheral_bh(peripheral, -1 * EFAULT);
	}
	SDL_Log("{blebug}---nativeConnectionStateChange, connected: %s", connected? "true": "false");
}

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativeConnectionStateChange(
    JNIEnv* env, jclass jcls,
    jstring jaddress, jboolean connected)
{
	const char* address = (*env)->GetStringUTFChars(env, jaddress, NULL);
	uint8_t uc6[SDL_BLE_MAC_ADDR_BYTES];
	mac_addr_str_2_uc6(address, uc6, ':');
	(*env)->ReleaseStringUTFChars(env, jaddress, address);

	PUSH_BLEDID(
		did->type = did_connectionstatechange;
		SDL_memcpy(did->ConnectionStateChange.address, uc6, SDL_BLE_MAC_ADDR_BYTES);
		did->ConnectionStateChange.connected = connected? SDL_TRUE: SDL_FALSE;
		);
}

void nativeServicesDiscovered(JNIEnv* env, jobject gatt, SDL_bool success)
{
	SDL_BlePeripheral* peripheral = connected_peripheral;
	if (!success) {
		if (current_callbacks && current_callbacks->discover_services) {
			current_callbacks->discover_services(peripheral, -1 * EFAULT);
		}
		return;
	}

	// if jobject is parameter, requrie call GetObjectClass, or result in below error:
	//     Class 'xxx' was optimized without verification; not verifying now

	// prepare PROPERTY_XXX
	jclass classClass = (*env)->FindClass(env, "android/bluetooth/BluetoothGattCharacteristic");
	jfieldID fid = (*env)->GetStaticFieldID(env, classClass, "PROPERTY_BROADCAST", "I");
	const int PROPERTY_BROADCAST = (*env)->GetStaticIntField(env, classClass, fid);
	fid = (*env)->GetStaticFieldID(env, classClass, "PROPERTY_READ", "I");
	const int PROPERTY_READ = (*env)->GetStaticIntField(env, classClass, fid);
	fid = (*env)->GetStaticFieldID(env, classClass, "PROPERTY_WRITE_NO_RESPONSE", "I");
	const int PROPERTY_WRITE_NO_RESPONSE = (*env)->GetStaticIntField(env, classClass, fid);
	fid = (*env)->GetStaticFieldID(env, classClass, "PROPERTY_WRITE", "I");
	const int PROPERTY_WRITE = (*env)->GetStaticIntField(env, classClass, fid);
	fid = (*env)->GetStaticFieldID(env, classClass, "PROPERTY_NOTIFY", "I");
	const int PROPERTY_NOTIFY = (*env)->GetStaticIntField(env, classClass, fid);
	fid = (*env)->GetStaticFieldID(env, classClass, "PROPERTY_INDICATE", "I");
	const int PROPERTY_INDICATE = (*env)->GetStaticIntField(env, classClass, fid);
	fid = (*env)->GetStaticFieldID(env, classClass, "PROPERTY_SIGNED_WRITE", "I");
	const int PROPERTY_SIGNED_WRITE = (*env)->GetStaticIntField(env, classClass, fid);
	fid = (*env)->GetStaticFieldID(env, classClass, "PROPERTY_EXTENDED_PROPS", "I");
	const int PROPERTY_EXTENDED_PROPS = (*env)->GetStaticIntField(env, classClass, fid);

	// List<BluetoothGattService> services = gatt.getServices();
	// int count = services.size();
	jmethodID mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, gatt), "getServices", "()Ljava/util/List;");
	jobject services = (*env)->CallObjectMethod(env, gatt, mid);
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, services), "size", "()I");
	int count = (*env)->CallIntMethod(env, services, mid);
	SDL_Log("nativeServicesDiscovered, service count:%i", count);

	discover_services_uh(peripheral, count);

	int at, at2;
	for (at = 0; at < count; at ++) {
		// BluetoothGattService service = services.get(at);
		// uuid = service.getUuid().toString();
		mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, services), "get", "(I)Ljava/lang/Object;");
		jobject service = (*env)->CallObjectMethod(env, services, mid, at);

		mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, service), "getUuid", "()Ljava/util/UUID;");
		jobject uuid = (*env)->CallObjectMethod(env, service, mid);
		mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, uuid), "toString", "()Ljava/lang/String;");
		jstring uuid_jstr = (*env)->CallObjectMethod(env, uuid, mid);

		const char* uuid_cstr = (*env)->GetStringUTFChars(env, uuid_jstr, 0);
		SDL_Log("nativeServicesDiscovered, service#%i/%i: %s", at, count, uuid_cstr);

		SDL_BleService* ble_service = peripheral->services + at;
		discover_services_bh(peripheral, ble_service, uuid_cstr, NULL);

		(*env)->ReleaseStringUTFChars(env, uuid_jstr, uuid_cstr);
		(*env)->DeleteLocalRef(env, uuid_jstr);

		//
		// discover characteristics of this service
		//

		// List<BluetoothGattCharacteristic> charas = service.getCharacteristics();
		// int count2 = charas.size();
		jmethodID mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, service), "getCharacteristics", "()Ljava/util/List;");
		jobject charas = (*env)->CallObjectMethod(env, service, mid);
		mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, charas), "size", "()I");
		int count2 = (*env)->CallIntMethod(env, charas, mid);

		ble_service = discover_characteristics_uh(peripheral, peripheral->services + at, count2);
		for (at2 = 0; at2 < count2; at2 ++) {
			// BluetoothGattCharacteristic chara = charas.get(at2);
			// uuid = chara.getUuid().toString();
			mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, charas), "get", "(I)Ljava/lang/Object;");
			jobject chara = (*env)->CallObjectMethod(env, charas, mid, at2);
			mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, chara), "getUuid", "()Ljava/util/UUID;");
			uuid = (*env)->CallObjectMethod(env, chara, mid);
			mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, uuid), "toString", "()Ljava/lang/String;");
			uuid_jstr = (*env)->CallObjectMethod(env, uuid, mid);
			uuid_cstr = (*env)->GetStringUTFChars(env, uuid_jstr, 0);

			SDL_BleCharacteristic* ble_characteristic = ble_service->characteristics + at2;
			discover_characteristics_bh(peripheral, ble_service, ble_characteristic, uuid_cstr, NULL /*(*env)->NewGlobalRef(env, chara)*/);

			(*env)->ReleaseStringUTFChars(env, uuid_jstr, uuid_cstr);
			(*env)->DeleteLocalRef(env, uuid_jstr);

			// int property2 = chara.getProperties();
			mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, chara), "getProperties", "()I");
			int property2 = (*env)->CallIntMethod(env, chara, mid);
			(*env)->DeleteLocalRef(env, chara);

			// parse property.
			if (property2 & PROPERTY_BROADCAST) {
				ble_characteristic->properties |= SDL_BleCharacteristicPropertyBroadcast;
			}
			if (property2 & PROPERTY_READ) {
				ble_characteristic->properties |= SDL_BleCharacteristicPropertyRead;
			}
			if (property2 & PROPERTY_WRITE_NO_RESPONSE) {
				ble_characteristic->properties |= SDL_BleCharacteristicPropertyWriteWithoutResponse;
			}
			if (property2 & PROPERTY_WRITE) {
				ble_characteristic->properties |= SDL_BleCharacteristicPropertyWrite;
			}
			if (property2 & PROPERTY_NOTIFY) {
				ble_characteristic->properties |= SDL_BleCharacteristicPropertyNotify;
			}
			if (property2 & PROPERTY_INDICATE) {
				ble_characteristic->properties |= SDL_BleCharacteristicPropertyIndicate;
			}
			if (property2 & PROPERTY_SIGNED_WRITE) {
				ble_characteristic->properties |= SDL_BleCharacteristicPropertyAuthenticatedSignedWrites;
			}
			if (property2 & PROPERTY_EXTENDED_PROPS) {
				ble_characteristic->properties |= SDL_BleCharacteristicPropertyExtendedProperties;
			}
			SDL_Log("nativeServicesDiscovered, characteristic#%i: %s, properties: 0x%x", at2, uuid_cstr, ble_characteristic->properties);
		}

		(*env)->DeleteLocalRef(env, service);
	}

	if (current_callbacks && current_callbacks->discover_services) {
		current_callbacks->discover_services(peripheral, 0);
	}

	// release global reference
	(*env)->DeleteGlobalRef(env, gatt);
}

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativeServicesDiscovered(
    JNIEnv* env, jclass jcls,
    jobject gatt, jboolean success)
{
	PUSH_BLEDID(
		did->type = did_servicesdiscovered;
		did->ServicesDiscovered.gatt = (*env)->NewGlobalRef(env, gatt);
		did->ServicesDiscovered.success = success? SDL_TRUE: SDL_FALSE;
		);
}

void nativeCharacteristicRead(JNIEnv* env, jobject chara, jbyteArray buffer, jboolean notify)
{
	jmethodID mid;

	// uuid = chara.getUuid();
	// uuid_str = uuid.toString();
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, chara), "getUuid", "()Ljava/util/UUID;");
	jobject uuid = (*env)->CallObjectMethod(env, chara, mid);
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, uuid), "toString", "()Ljava/lang/String;");
	jstring chara_uuid_jstr = (*env)->CallObjectMethod(env, uuid, mid);

	// service = chara.getService();
	// uuid = chara.getUuid();
	// uuid_str = uuid.toString();
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, chara), "getService", "()Landroid/bluetooth/BluetoothGattService;");
	jobject service = (*env)->CallObjectMethod(env, chara, mid);
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, service), "getUuid", "()Ljava/util/UUID;");
	uuid = (*env)->CallObjectMethod(env, service, mid);
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, uuid), "toString", "()Ljava/lang/String;");
	jstring service_uuid_jstr = (*env)->CallObjectMethod(env, uuid, mid);

	const char* service_uuid_cstr = (*env)->GetStringUTFChars(env, service_uuid_jstr, 0);
	const char* chara_uuid_cstr = (*env)->GetStringUTFChars(env, chara_uuid_jstr, 0);

	jbyte* elements = (*env)->GetByteArrayElements(env, buffer, NULL);
	uint8_t* data = (uint8_t*)elements;
	int len = (*env)->GetArrayLength(env, (jbyteArray)buffer);

	SDL_BlePeripheral* peripheral = connected_peripheral;
	if (current_callbacks && current_callbacks->read_characteristic) {
		current_callbacks->read_characteristic(peripheral, find_characteristic_from_uuid(peripheral, service_uuid_cstr, chara_uuid_cstr), data, len);
	}
	(*env)->ReleaseByteArrayElements(env, buffer, elements, JNI_ABORT);

	(*env)->ReleaseStringUTFChars(env, service_uuid_jstr, service_uuid_cstr);
	(*env)->ReleaseStringUTFChars(env, chara_uuid_jstr, chara_uuid_cstr);

	(*env)->DeleteLocalRef(env, service_uuid_jstr);
	(*env)->DeleteLocalRef(env, chara_uuid_jstr);

	// release global reference
	(*env)->DeleteGlobalRef(env, chara);
	(*env)->DeleteGlobalRef(env, buffer);
}

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativeCharacteristicRead(
    JNIEnv* env, jclass jcls,
    jobject chara, jbyteArray buffer, jboolean notify)
{
	PUSH_BLEDID(
		did->type = did_characteristicread;
		did->CharacteristicRead.chara = (*env)->NewGlobalRef(env, chara);
		did->CharacteristicRead.buffer = (*env)->NewGlobalRef(env, buffer);
		did->CharacteristicRead.notify = notify? SDL_TRUE: SDL_FALSE;
		);
}

void nativeCharacteristicWrite(JNIEnv* env, jobject chara, jboolean success)
{
	jmethodID mid;

	// uuid = chara.getUuid();
	// uuid_str = uuid.toString();
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, chara), "getUuid", "()Ljava/util/UUID;");
	jobject uuid = (*env)->CallObjectMethod(env, chara, mid);
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, uuid), "toString", "()Ljava/lang/String;");
	jstring chara_uuid_jstr = (*env)->CallObjectMethod(env, uuid, mid);

	// service = chara.getService();
	// uuid = chara.getUuid();
	// uuid_str = uuid.toString();
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, chara), "getService", "()Landroid/bluetooth/BluetoothGattService;");
	jobject service = (*env)->CallObjectMethod(env, chara, mid);
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, service), "getUuid", "()Ljava/util/UUID;");
	uuid = (*env)->CallObjectMethod(env, service, mid);
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, uuid), "toString", "()Ljava/lang/String;");
	jstring service_uuid_jstr = (*env)->CallObjectMethod(env, uuid, mid);

	const char* service_uuid_cstr = (*env)->GetStringUTFChars(env, service_uuid_jstr, 0);
	const char* chara_uuid_cstr = (*env)->GetStringUTFChars(env, chara_uuid_jstr, 0);

	SDL_BlePeripheral* peripheral = connected_peripheral;
	if (current_callbacks && current_callbacks->write_characteristic) {
		current_callbacks->write_characteristic(peripheral, find_characteristic_from_uuid(peripheral, service_uuid_cstr, chara_uuid_cstr), success? 0: -1 * EFAULT);
	}

	(*env)->ReleaseStringUTFChars(env, service_uuid_jstr, service_uuid_cstr);
	(*env)->ReleaseStringUTFChars(env, chara_uuid_jstr, chara_uuid_cstr);

	(*env)->DeleteLocalRef(env, service_uuid_jstr);
	(*env)->DeleteLocalRef(env, chara_uuid_jstr);

	// release global reference
	(*env)->DeleteGlobalRef(env, chara);
}

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativeCharacteristicWrite(
    JNIEnv* env, jclass jcls,
    jobject chara, jboolean success)
{
	PUSH_BLEDID(
		did->type = did_characteristicwrite;
		did->CharacteristicWrite.chara = (*env)->NewGlobalRef(env, chara);
		did->CharacteristicWrite.success = success? SDL_TRUE: SDL_FALSE;
		);
}

void nativeDescriptorWrite(JNIEnv* env, jobject descriptor)
{
	jmethodID mid;

	// uuid = descriptor.getUuid();
	// descriptor_uuid_str = uuid.toString();
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, descriptor), "getUuid", "()Ljava/util/UUID;");
	jobject uuid = (*env)->CallObjectMethod(env, descriptor, mid);
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, uuid), "toString", "()Ljava/lang/String;");
	jstring descriptor_uuid_jstr = (*env)->CallObjectMethod(env, uuid, mid);

	// chara = descriptor.getCharacteristic();
	// uuid = chara.getUuid();
	// uuid_str = uuid.toString();
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, descriptor), "getCharacteristic", "()Landroid/bluetooth/BluetoothGattCharacteristic;");
	jobject chara = (*env)->CallObjectMethod(env, descriptor, mid);
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, chara), "getUuid", "()Ljava/util/UUID;");
	uuid = (*env)->CallObjectMethod(env, chara, mid);
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, uuid), "toString", "()Ljava/lang/String;");
	jstring chara_uuid_jstr = (*env)->CallObjectMethod(env, uuid, mid);

	// service = chara.getService();
	// uuid = chara.getUuid();
	// uuid_str = uuid.toString();
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, chara), "getService", "()Landroid/bluetooth/BluetoothGattService;");
	jobject service = (*env)->CallObjectMethod(env, chara, mid);
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, service), "getUuid", "()Ljava/util/UUID;");
	uuid = (*env)->CallObjectMethod(env, service, mid);
	mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, uuid), "toString", "()Ljava/lang/String;");
	jstring service_uuid_jstr = (*env)->CallObjectMethod(env, uuid, mid);

	const char* service_uuid_cstr = (*env)->GetStringUTFChars(env, service_uuid_jstr, 0);
	const char* chara_uuid_cstr = (*env)->GetStringUTFChars(env, chara_uuid_jstr, 0);
	const char* descriptor_uuid_cstr = (*env)->GetStringUTFChars(env, descriptor_uuid_jstr, 0);

	if (SDL_BleUuidEqual(descriptor_uuid_cstr, UUID_CLIENT_CHARACTERISTIC_CONFIG)) {
		// this is notify
		SDL_BlePeripheral* peripheral = connected_peripheral;
		if (current_callbacks && current_callbacks->notify_characteristic) {
			current_callbacks->notify_characteristic(peripheral, find_characteristic_from_uuid(peripheral, service_uuid_cstr, chara_uuid_cstr), 0);
		}
	}
	(*env)->ReleaseStringUTFChars(env, service_uuid_jstr, service_uuid_cstr);
	(*env)->ReleaseStringUTFChars(env, chara_uuid_jstr, chara_uuid_cstr);
	(*env)->ReleaseStringUTFChars(env, descriptor_uuid_jstr, descriptor_uuid_cstr);

	(*env)->DeleteLocalRef(env, service_uuid_jstr);
	(*env)->DeleteLocalRef(env, chara_uuid_jstr);
	(*env)->DeleteLocalRef(env, descriptor_uuid_jstr);

	// release global reference
	(*env)->DeleteGlobalRef(env, descriptor);
}

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativeDescriptorWrite(
    JNIEnv* env, jclass jcls,
    jobject descriptor)
{
	PUSH_BLEDID(
		did->type = did_descriptorwrite;
		did->DescriptorWrite.descriptor = (*env)->NewGlobalRef(env, descriptor);
		);
}

static void Android_ScanPeripherals(const char* uuid)
{
	if (!center_initialized) {
		center_initialized = Android_JNI_BleInitialize();
	}
	if (!center_initialized) {
		return;
	}
	Android_JNI_BleScanPeripherals(uuid);
}

static void Android_StopScanPeripherals(void)
{
	if (!center_initialized) {
		return;
	}
    Android_JNI_BleStopScanPeripherals();
}

static void Android_ConnectPeripheral(SDL_BlePeripheral* peripheral)
{
	if (!center_initialized) {
		return;
	}

	char* address = mac_addr_uc6_2_str(peripheral->mac_addr, ':');
	Android_JNI_BleConnectPeripheral(address);
	SDL_free(address);
}

static void Android_DisconnectPeripheral(SDL_BlePeripheral* peripheral)
{
	if (!center_initialized) {
		return;
	}

	if (peripheral != NULL) {
		char* address = mac_addr_uc6_2_str(peripheral->mac_addr, ':');
		Android_JNI_BleDisconnectPeripheral(address);
		SDL_free(address);
	} else {
		// special case: make sure close 'pending' connect
		Android_JNI_BleDisconnectPeripheral(NULL);
	}
}

static void Android_GetServices(SDL_BlePeripheral* peripheral)
{
	if (!center_initialized) {
		return;
	}

	char* address = mac_addr_uc6_2_str(peripheral->mac_addr, ':');
	Android_JNI_BleDiscoverServices(address);
	SDL_free(address);
}

static void Android_ClearCache(SDL_BlePeripheral* peripheral)
{
	if (!center_initialized) {
		return;
	}

	char* address = mac_addr_uc6_2_str(peripheral->mac_addr, ':');
	Android_JNI_BleClearCache(address);
	SDL_free(address);
}

static void Android_GetCharacteristics(const SDL_BlePeripheral* peripheral, SDL_BleService* service)
{
	if (peripheral != connected_peripheral) {
		return;
	}
	if (current_callbacks && current_callbacks->discover_characteristics) {
        current_callbacks->discover_characteristics(connected_peripheral, service, 0);
    }
}

static void Android_ReadCharacteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic)
{
	if (!center_initialized) {
		return;
	}

	char* address = mac_addr_uc6_2_str(peripheral->mac_addr, ':');
	Android_JNI_BleReadCharacteristic(address, characteristic->service->uuid, characteristic->uuid);
	SDL_free(address);
}

static void Android_NotifyCharacteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, SDL_bool enable)
{
	if (!center_initialized) {
		return;
	}

	char* address = mac_addr_uc6_2_str(peripheral->mac_addr, ':');
	Android_JNI_BleNotifyCharacteristic(address, characteristic->service->uuid, characteristic->uuid, enable);
	SDL_free(address);
}

static void Android_WriteCharacteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const unsigned char* data, int size)
{
	if (!center_initialized) {
		return;
	}

	char* address = mac_addr_uc6_2_str(peripheral->mac_addr, ':');
	Android_JNI_BleWriteCharacteristic(address, characteristic->service->uuid, characteristic->uuid, data, size);
	SDL_free(address);
}

static void Android_ReleaseCharacteristicCookie(const SDL_BleCharacteristic* characteristic, int at)
{
	SDL_Log("Android_ReleaseCharacteristicCookie, cookie: %p", characteristic->cookie);
	JNIEnv* env = Android_JNI_GetEnv();
	(*env)->DeleteGlobalRef(env, characteristic->cookie);
}

//
// use as peripheral
//
void nativePConnectionStateChange(JNIEnv* env, uint8_t* address, SDL_bool connected)
{
	if (connected) {
		pconnect_center_bh(address, 0);
	} else {
		// On android, think all disconnect to except disconnect.
		// SDL_Log("nativePConnectionStateChange, will call disconnect_peripheral_bh");
		pdisconnect_center_bh();
	}
}

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativePConnectionStateChange(
    JNIEnv* env, jclass jcls,
    jstring address, jboolean connected)
{
	// if disconnected(connected = false), address is NULL.
	const char *address_ptr = NULL;
	if (address != NULL) {
		address_ptr = (*env)->GetStringUTFChars(env, address, NULL);
	}
	// char* address2 = SDL_strdup(address_ptr);
	PUSH_BLEDID(
		did->type = did_pconnectionstatechange;
		mac_addr_str_2_uc6(address_ptr, did->PConnectionStateChange.address, ':');
		did->PConnectionStateChange.connected = connected? SDL_TRUE: SDL_FALSE;
		);
	if (address != NULL) {
		(*env)->ReleaseStringUTFChars(env, address, address_ptr);
	}
}

void nativePCharacteristicRead(JNIEnv* env, char* chara, jbyteArray buffer, jboolean notify)
{
	jbyte* elements = (*env)->GetByteArrayElements(env, buffer, NULL);
	uint8_t* data = (uint8_t*)elements;
	int len = (*env)->GetArrayLength(env, (jbyteArray)buffer);

	if (current_pcallbacks && current_pcallbacks->read_characteristic) {
		current_pcallbacks->read_characteristic(chara, data, len);
	}
	(*env)->ReleaseByteArrayElements(env, buffer, elements, JNI_ABORT);

	// release global reference
	(*env)->DeleteGlobalRef(env, buffer);
	SDL_free(chara);
}

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativePCharacteristicRead(
    JNIEnv* env, jclass jcls,
    jstring chara, jbyteArray buffer, jboolean notify)
{
	const char *chara_ptr = (*env)->GetStringUTFChars(env, chara, NULL);
	PUSH_BLEDID(
		did->type = did_pcharacteristicread;
		did->PCharacteristicRead.chara = SDL_strdup(chara_ptr);
		did->PCharacteristicRead.buffer = (*env)->NewGlobalRef(env, buffer);
		did->PCharacteristicRead.notify = notify? SDL_TRUE: SDL_FALSE;
		);
	(*env)->ReleaseStringUTFChars(env, chara, chara_ptr);
}

void nativePNotificationSent(JNIEnv* env, char* chara, int status)
{
	if (current_pcallbacks && current_pcallbacks->notification_sent) {
		current_pcallbacks->notification_sent(chara, status);
	}
	SDL_free(chara);
}

JNIEXPORT void JNICALL Java_org_librose_SDLBle_nativePNotificationSent(
    JNIEnv* env, jclass jcls,
    jstring chara, jint status)
{
	const char *chara_ptr = (*env)->GetStringUTFChars(env, chara, NULL);
	PUSH_BLEDID(
		did->type = did_pnotificationsent;
		did->PNotificationSent.chara = SDL_strdup(chara_ptr);
		did->PNotificationSent.status = status;
		);
	(*env)->ReleaseStringUTFChars(env, chara, chara_ptr);
}


static void Android_PStartAdvertising(const char* service_uuid, const char* name, int manufacturer_id, const uint8_t* manufacturer_data, int manufacturer_data_size)
{
	Android_JNI_PBleStartAdvertising(service_uuid, name, manufacturer_id, manufacturer_data, manufacturer_data_size);
}

static void Android_PStopAdvertising(void)
{
	Android_JNI_PBleStopAdvertising();
}

static SDL_bool Android_PIsAdvertising(void)
{
	return Android_JNI_PBleIsAdvertising();
}

static void Android_PWriteCharacteristic(const char* uuid, const uint8_t* data, int size)
{
	Android_JNI_PBleWriteCharacteristic(uuid, data, size);
}

static void Android_PCancelConnection(void)
{
	Android_JNI_PBleCancelConnection();
}

static void Android_Quit(void)
{
	SDL_DestroyMutex(bledid_lock);
	bledid_lock = NULL;

	if (adapter) {
		JNIEnv* env = Android_JNI_GetEnv();
		(*env)->DeleteGlobalRef(env, adapter);
		adapter = NULL;
	}
}

// Windows driver bootstrap functions
static int Android_Available(void)
{
	return (1);
}

static SDL_MiniBle* Android_CreateBle(void)
{
	SDL_MiniBle *ble;

	// Initialize all variables that we clean on shutdown
    ble = (SDL_MiniBle *)SDL_calloc(1, sizeof(SDL_MiniBle));
	if (!ble) {
		SDL_OutOfMemory();
		return (0);
	}

	// Set the function pointers
	ble->ScanPeripherals = Android_ScanPeripherals;
	ble->StopScanPeripherals = Android_StopScanPeripherals;
	ble->ConnectPeripheral = Android_ConnectPeripheral;
	ble->DisconnectPeripheral = Android_DisconnectPeripheral;
	ble->GetServices = Android_GetServices;
	ble->ClearCache = Android_ClearCache;
	ble->GetCharacteristics = Android_GetCharacteristics;
	ble->ReadCharacteristic = Android_ReadCharacteristic;
	ble->NotifyCharacteristic = Android_NotifyCharacteristic;
	ble->WriteCharacteristic = Android_WriteCharacteristic;
	ble->ReleaseCharacteristicCookie = Android_ReleaseCharacteristicCookie;
	ble->Quit = Android_Quit;

	ble->PStartAdvertising = Android_PStartAdvertising;
	ble->PStopAdvertising = Android_PStopAdvertising;
	ble->PIsAdvertising = Android_PIsAdvertising;
	ble->PWriteCharacteristic = Android_PWriteCharacteristic;
	ble->PCancelConnection = Android_PCancelConnection;

	bledid_lock = SDL_CreateMutex();
    return ble;
}


BleBootStrap Android_ble = {
	Android_Available, Android_CreateBle
};

