/* Copyright 2008-2009 Uppsala University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <libcpphaggle/Platform.h>

#include <assert.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <haggleutils.h>

#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <sys/stat.h>
#endif

#include "XMLMetadata.h"
#include "DataObject.h"
#include "Trace.h"

// This include is for the HAGGLE_ATTR_CONTROL_NAME attribute name. 
// TODO: remove dependency on libhaggle header
#include "../libhaggle/include/libhaggle/ipc.h"

unsigned int DataObject::totNum = 0;

// This is used in putData():
typedef struct pDd_s {
	// The metadata header
	unsigned char *header;
	// The current length of the metadata header (above):
	size_t header_len;
	// The current length of the metadata header pointer (above):
	size_t header_alloc_len;
	// File pointer to the file where the data object's data is stored:
	FILE *fp;
	// The amount of data left to write to the data file:
	size_t bytes_left;
} *pDd;

// Creates and initializes a pDd data structure.
static pDd create_pDd(void)
{
	pDd retval;
	
	retval = (pDd) malloc(sizeof(struct pDd_s));
	
	if (!retval)
		return NULL;

	retval->header = NULL;
	retval->header_len = 0;
	retval->header_alloc_len = 0;
	retval->fp = NULL;
	retval->bytes_left = 0;
	
	return retval;
}

// Releases the header part of a pDd structure.
static void free_pDd_header(pDd data)
{
	if (!data)
		return;

	data->header_len = 0;
	data->header_alloc_len = 0;
	free(data->header);
	// Let's not have any lingering pointers to dead data:
	data->header = NULL;
}

// Releases an entire pDd structure.
void DataObject::free_pDd(void)
{
	pDd data = (pDd) putData_data;
	if (!data)
		return;
	
	if(data->header != NULL)
		free(data->header);
	if(data->fp != NULL)
		fclose(data->fp);
	free(data);
	// Let's not have any lingering pointers to dead data:
	putData_data = NULL;
}

DataObject::DataObject(InterfaceRef _localIface, InterfaceRef _remoteIface, const string _storagepath) : 
#ifdef DEBUG_LEAKS
                LeakMonitor(LEAK_TYPE_DATAOBJECT),
#endif
                signatureStatus(DATAOBJECT_SIGNATURE_MISSING),
                signee(""), signature(NULL), signature_len(0), num(totNum++), 
                metadata(NULL), filename(""), filepath(""), isForLocalApp(false), 
                ownsFile(true), storagepath(_storagepath),
                dataLen(0), dynamicDataLen(false), createTime(Timeval::now()),
		has_CreateTime(false), receiveTime(-1), 
                localIface(_localIface), remoteIface(_remoteIface), rxTime(0), 
                persistent(true), duplicate(false), isNodeDesc(false), isThisNodeDesc(false),
                putData_data(NULL), hasDataHash(false), dataState(DATA_STATE_NO_DATA)
{
	putData_data = (void *) create_pDd();

#if HAVE_EXCEPTION
	if (putData_data == NULL)
		throw DataObjectException(-1, "Could not allocate memory");
#endif
}

DataObject::DataObject(const char *raw, const unsigned long len, InterfaceRef _localIface, InterfaceRef _remoteIface, const string _storagepath) : 
#ifdef DEBUG_LEAKS
                LeakMonitor(LEAK_TYPE_DATAOBJECT),
#endif 
                signatureStatus(DATAOBJECT_SIGNATURE_MISSING),
                signee(""), signature(NULL), signature_len(0), num(totNum++),
                metadata(NULL), filename(""), filepath(""), isForLocalApp(false), 
                ownsFile(true), storagepath(_storagepath),
                dataLen(0), dynamicDataLen(false), createTime(Timeval::now()), 
		has_CreateTime(false), receiveTime(-1), 
                localIface(_localIface), remoteIface(_remoteIface), rxTime(0), 
                persistent(true), duplicate(false), isNodeDesc(false), isThisNodeDesc(false),
                putData_data(NULL), hasDataHash(false), dataState(DATA_STATE_NO_DATA)
{
	if (!raw) {
                if (!initMetadata()) {
                        HAGGLE_ERR("Could not init metadata\n");
                        return;
                }
	} else {
	
                metadata = new XMLMetadata(raw, len);
                
                if (!metadata || metadata->getName() != "Haggle") {
                        if (metadata) {
                                HAGGLE_ERR("Could not create metadata\n");
                                delete metadata;
                                metadata = NULL;
                        }
                        return;
                }
	}
        
	if (parseMetadata() < 0) {
		delete metadata;
		metadata = NULL;
#if HAVE_EXCEPTION
		throw DataObjectException(-1, "Could not parse metadata");
#endif
	}
}

// Copy constructor
DataObject::DataObject(const DataObject& dObj) : 
#ifdef DEBUG_LEAKS
                LeakMonitor(LEAK_TYPE_DATAOBJECT),
#endif
                signatureStatus(dObj.signatureStatus),
                signee(dObj.signee), signature(NULL), signature_len(dObj.signature_len), 
		num(totNum++), metadata(dObj.metadata ? dObj.metadata->copy() : NULL), 
                attrs(dObj.attrs),
                filename(dObj.filename), filepath(dObj.filepath), 
                isForLocalApp(dObj.isForLocalApp), ownsFile(false), 
                storagepath(dObj.storagepath),
                dataLen(dObj.dataLen), dynamicDataLen(dObj.dynamicDataLen), 
                createTime(dObj.createTime), has_CreateTime(dObj.has_CreateTime),
		receiveTime(dObj.receiveTime), 
                localIface(dObj.localIface), remoteIface(dObj.remoteIface), 
                rxTime(dObj.rxTime), persistent(dObj.persistent), 
                duplicate(false), isNodeDesc(dObj.isNodeDesc), isThisNodeDesc(dObj.isThisNodeDesc),
                putData_data(NULL), hasDataHash(dObj.hasDataHash), dataState(dObj.dataState)
{
	memcpy(id, dObj.id, DATAOBJECT_ID_LEN);
	memcpy(idStr, dObj.idStr, MAX_DATAOBJECT_ID_STR_LEN);
	memcpy(dataHash, dObj.dataHash, SHA_DIGEST_LENGTH);
	
	if (dObj.signature && signature_len) {
		signature = (unsigned char *)malloc(signature_len);
		
		if (signature) 
			memcpy(signature, dObj.signature, signature_len);
	}
}

DataObject::~DataObject()
{
	if (putData_data) {
		free_pDd();
	}
	if (metadata) {
                delete metadata;
	}
	if (ownsFile) {
		if (filepath.size() > 0) {
#if defined(OS_WINDOWS_MOBILE)
			// FIXME: does this really work?!
			DeleteFile((LPCWSTR) ("\\\\?\\" + filepath).c_str());
#else
			remove(filepath.c_str());
#endif 
		}
        }
	
	if (signature)
		free(signature);
}

DataObject *DataObject::copy() const 
{
		return new DataObject(*this);
}

bool DataObject::isValid() const
{
	return metadata != NULL;
}

bool DataObject::initMetadata()
{
        if (metadata)
                return false;

        metadata = new XMLMetadata("Haggle");
        
        if (!metadata)
                return false;
        
        return true;
}

const unsigned char *DataObject::getSignature() const
{
	return signature;
}

void DataObject::setSignature(const string signee, unsigned char *sig, size_t siglen)
{
	if (signature)
		free(signature);
	
	this->signee = signee;
	signature = sig;
	signature_len = siglen;
	signatureStatus = DATAOBJECT_SIGNATURE_UNVERIFIED;
	
	HAGGLE_DBG("Set signature on data object, siglen=%lu\n", siglen);
}

bool DataObject::shouldSign() const
{
	return (!isSigned() && !getAttribute(HAGGLE_ATTR_CONTROL_NAME));
}

void DataObject::createFilePath()
{
#define PATH_LENGTH 256
        char path[PATH_LENGTH];
        long i;
        FILE *fp;
	
        i = 0;
        // Try to just use the sha1 hash value for the filename
        snprintf(path, PATH_LENGTH, "%s%s%s", 
                 storagepath.c_str(), PLATFORM_PATH_DELIMITER, 
                 getFileName().c_str());
        
        do {
                // Is there already a file there?
                fp = fopen(path, "rb");
                
                if (fp != NULL) {
                        // Yep.
			
                        // Close that file:
                        fclose(fp);
			
                        // Generate a new filename:
                        i++;
                        sprintf(path, "%s%s%ld-%s", 
                                storagepath.c_str(), 
                                PLATFORM_PATH_DELIMITER,
                                i, 
                                getFileName().c_str());
                }
        } while (fp != NULL);

        // Make sure the file path is the same as the file path
        // written to:
        filepath = path;
}

Metadata *DataObject::getOrCreateDataMetadata()
{
        if (!metadata)
                return NULL;

        Metadata *md = metadata->getMetadata(DATAOBJECT_METADATA_DATA);

        if (!md) {
                return metadata->addMetadata(DATAOBJECT_METADATA_DATA);
        }
        return md;
}

void DataObject::setThumbnail(char *data, long len)
{
	Metadata *md = getOrCreateDataMetadata();
	char *b64 = NULL;
	
	if(!md)
		return;
	
	if(base64_encode_alloc(data, len, &b64) == 0)
		return;
	
	string str = "";
	
	str += b64;
	free(b64);
	
	// FIXME: does not caring about the return value leak memory?
	md->addMetadata("Thumbnail", str);
}

void DataObject::setFileName(const string fn)
{        
        filename = fn;
}

void DataObject::setFilePath(const string fp)
{        
        filepath = fp;
	// Assume that the file path actually points to some data and mark it as unverified.
	// FIXME: Maybe store verified state in data store and update that as well?
	dataState = DATA_STATE_NOT_VERIFIED;
}

void DataObject::setIsForLocalApp(const bool val)
{
        isForLocalApp = val;
}

void DataObject::setDataLen(const size_t _dataLen)
{        
        dataLen = _dataLen;
	// FIXME: Maybe store verified state in data store and update that as well?
        if (dataLen > 0)
                dataState = DATA_STATE_NOT_VERIFIED;
}

ssize_t DataObject::putData(void *_data, size_t len, size_t *remaining)
{
        pDd info = (pDd) putData_data;
        unsigned char *data = (unsigned char *)_data;
        ssize_t putLen = 0;
	
        if (!remaining)
                return -1;
        
        if (len == 0)
                return 0;
        
        *remaining = DATAOBJECT_METADATA_PENDING;
	
        // Is this data object being built?
        if (putData_data == NULL) {
                // Put on a finished data object should return 0 put bytes.
                *remaining = 0;
                return 0;
        }
	
        // Has the metadata been filled in yet?
        if (metadata == NULL) {
                
                // No. Insert the bytes given into the header buffer first:
                /*
                  FIXME: this function searches for the XML end tag </haggle> (or
                  <haggle ... />) to determine where the metadata ends. This REQUIRES
                  the use of XML as metadata, or this function will never stop adding
                  to the header.
                */

		if (info->header_len + len > info->header_alloc_len) {
			unsigned char *tmp;
			/* We allocate a larger chunk of memory to put the header data into 
			and then hope the header fits. If the chunk proves to be too small, 
			we increase the size in a future put call.
			*/
			tmp = (unsigned char *)realloc(info->header, info->header_len + len + 1024);
			
                        if (tmp == NULL)
                                return -1;

                        info->header = tmp;
                        info->header_alloc_len = info->header_len + len + 1024;
		}
		
                // Add the data, byte for byte:
                while (len > 0 && !metadata) {
                        // Insert another byte into the header:
                        info->header[info->header_len] = data[0];
                        info->header_len++;
                        data++;
                        putLen++;
                        len--;
			
                        // Is this the end of the header?
                        // FIXME: find a way to search for the <haggle ... /> style header.
                        if (info->header_len >= 9) {
                                if (info->header[info->header_len - 9] == '<' && info->header[info->header_len - 8] == '/'
                                    && (info->header[info->header_len - 7] == 'H' || info->header[info->header_len - 7] == 'h')
                                    && (info->header[info->header_len - 6] == 'A' || info->header[info->header_len - 6] == 'a')
                                    && (info->header[info->header_len - 5] == 'G' || info->header[info->header_len - 5] == 'g')
                                    && (info->header[info->header_len - 4] == 'G' || info->header[info->header_len - 4] == 'g')
                                    && (info->header[info->header_len - 3] == 'L' || info->header[info->header_len - 3] == 'l')
                                    && (info->header[info->header_len - 2] == 'E' || info->header[info->header_len - 2] == 'e')
                                    && info->header[info->header_len - 1] == '>') {
                                        // Yes. Yay!

                                        metadata = new XMLMetadata((const char *)info->header, info->header_len);

                                        if (!metadata) {
						free_pDd_header(info);
                                                HAGGLE_ERR("Caught XML exception\n");
                                                return -1;
                                        }
                                        if (metadata->getName() != "Haggle") {
						free_pDd_header(info);
                                                HAGGLE_ERR("Metadata not recognized\n");
                                                delete metadata;
						metadata = NULL;
                                                return -1;
                                        }

                                        if (parseMetadata() < 0) {
						free_pDd_header(info);
                                                HAGGLE_ERR("Parse metadata on new data object failed\n");
						delete metadata;
						metadata = NULL;
                                                return -1;
                                        }
					free_pDd_header(info);
                                }
                        }
                }
        }	
        // Is the file to write into already open?
        if (info->fp == NULL && metadata) {

                // Figure out how many bytes should be put into the file:
                info->bytes_left = getDataLen();

                *remaining = info->bytes_left;

                HAGGLE_DBG("Going to put %lu bytes into file %s\n", 
                           info->bytes_left, getFilePath().c_str());

                // Any bytes at all?
                if (info->bytes_left == 0) {
                        // Nope.

                        // We're done with the info:
                        free_pDd();
                        *remaining = 0;
                        return putLen;
                }
                // Create the path and file were we store the data.
                createFilePath();
                // Open the file for writing:
                info->fp = fopen(getFilePath().c_str(), "wb");
		
                if (info->fp == NULL) {
						free_pDd();
                        HAGGLE_ERR("Could not open %s for writing data object data\n", 
                                   getFilePath().c_str());
                        return -1;
                }
		// Mark the data state as not verified yet.
		dataState = DATA_STATE_NOT_VERIFIED;
        }
        // If we just finished putting the metadata header, then len will be
        // zero and we should return the amount put.
        if (len == 0)
                return putLen;

        // Is this less data than what is left?
        if (info->bytes_left > len) {
                // Write all the data to the file:
                size_t nitems = fwrite(data, len, 1, info->fp);
		
                if (nitems != 1) {
                        // TODO: check error values with, e.g., ferror()?
                        HAGGLE_ERR("Error-1 on writing %lu bytes to file %s, nitems=%lu\n", 
                                   len, getFilePath().c_str(), nitems);
						free_pDd();
                        return -1;
                }

                putLen = len;

                // Decrease the amount of data left:
                info->bytes_left -= putLen;
                // Return the number of bytes left to write:
                *remaining = info->bytes_left;
        } else if (info->bytes_left > 0) {
		
                // Write the last bytes of the actual data object:
                size_t nitems = fwrite(data, info->bytes_left, 1, info->fp);
                // We're done with the file now:
				
                if (nitems != 1) {
                        // TODO: check error values with, e.g., ferror()?
                        HAGGLE_ERR("Error-2 on writing %lu bytes to file %s, nitems=%lu\n", 
                                   info->bytes_left, getFilePath().c_str(), nitems);
						free_pDd();
                        return -1;
                }
                fclose(info->fp);
                info->fp = NULL;
		
                putLen = info->bytes_left;

                info->bytes_left -= putLen;
                *remaining = info->bytes_left;

		free_pDd();
        }
	
        return putLen;
}

class DataObjectDataRetrieverImplementation : public DataObjectDataRetriever {
    public:
        /**
           The data object. We keep a reference to it to make sure it isn't 
           deleted. Just in case it would be in charge of deleting its file or 
           something.
        */
        DataObjectRef dObj;
        /// The metadata header
        unsigned char *header;
        /// The length of the metadata header (above):
        size_t header_len;
        /// File pointer to the file where the data object's data is stored:
        FILE *fp;
        /// The amount of header data left to read:
        size_t header_bytes_left;
        /// The amount of data left to read from the data file:
        size_t bytes_left;
	
        DataObjectDataRetrieverImplementation(DataObjectRef _dObj);
        ~DataObjectDataRetrieverImplementation();
	
        ssize_t retrieve(void *data, size_t len, bool getHeaderOnly);
};

DataObjectDataRetrieverImplementation::DataObjectDataRetrieverImplementation(DataObjectRef _dObj) :
                dObj(_dObj), header(NULL), header_len(0), fp(NULL), header_bytes_left(0), bytes_left(0)
{ 
         if (dObj->getDataLen() > 0 || dObj->getDynamicDataLen()) {
                
                fp = fopen(dObj->getFilePath().c_str(), "rb");

                if (fp == NULL) {
                        HAGGLE_ERR("ERROR: Unable to open file \"%s\". dataLen=%lu dynamicDataLen=%s\n", 
                                   dObj->getFilePath().c_str(), dObj->getDataLen(), dObj->getDynamicDataLen() ? "true" : "false");
                        // Unable to open file - fail:
                        goto fail_open;
                }
		
                // Should the data file size be figured out here?
                if (dObj->getDynamicDataLen()) {
                        // Find file size:
                        fseek(fp, 0L, SEEK_END);
                        bytes_left = ftell(fp);
                        fseek(fp, 0L, SEEK_SET);
			dObj->setDataLen(bytes_left);
                } else
                        bytes_left = dObj->getDataLen();
        } else {
                fp = NULL;
                bytes_left = 0;
        }

        // Find header size:
        if (!dObj->getRawMetadataAlloc((char **) &(header), (size_t *)&(header_len))) {
                HAGGLE_ERR("ERROR: Unable to retrieve header.\n");
                goto fail_header;
        }
	
        // Remove trailing characters up to the end of the metadata:
        while (header[header_len-1] != '>' && header_len) {
                header_len--;
        }
	
        // The entire header is left to read:
        header_bytes_left = header_len;

        // Succeeded!
        return;

fail_header:
        // Close the file:
        if (fp != NULL)
                fclose(fp);
fail_open:
        // Failed!
#if HAVE_EXCEPTION
        throw Exception(0, (char *) "Unable to start getting data!\n");
#endif
        return;
}

DataObjectDataRetrieverImplementation::~DataObjectDataRetrieverImplementation()
{
        // Close the file:
        if (fp != NULL)
                fclose(fp);

        // We must free the metadata header
        if (header)
                free(header);
}

DataObjectDataRetrieverRef DataObject::getDataObjectDataRetriever(void)
{
       return new DataObjectDataRetrieverImplementation(this);
}

ssize_t DataObjectDataRetrieverImplementation::retrieve(void *data, size_t len, bool getHeaderOnly)
{
        size_t readLen = 0;
	
        // Can we fill in that buffer?
        if (len == 0)
                // NO: can't do the job:
                return 0;
	
        // Is there anything left to read in the header?
        if (header_bytes_left) {
                // Yep.
		
                // Is there enough header left to fill the buffer, or more?
                if (header_bytes_left >= len) {
                        // Yep. Fill the buffer:
                        memcpy(data, &header[header_len - header_bytes_left], len);
                        header_bytes_left -= len;
                        return len;
                } else {
                        // Nope. Fill the buffer with the rest of the header:
                        readLen = header_bytes_left;
			
                        memcpy(data, &header[header_len - header_bytes_left], header_bytes_left);
                        data = (char *)data + header_bytes_left;
                        len -= header_bytes_left;
                        header_bytes_left = 0;
                }
        }
		if(getHeaderOnly)
			return readLen;
        if (!fp) {
                return readLen;
        }
	
        // Make sure we don't try to read too much data:
        if (len > bytes_left)
                len = bytes_left;
	
        // Read data.
        size_t fstart = ftell(fp);
        size_t nitems = fread(data, len, 1, fp);
        size_t fstop = ftell(fp);
	
        readLen += (fstop - fstart);
        // Count down the amount of data left to read:
        bytes_left -= (fstop - fstart);
	
        // Are we done?
        if (bytes_left == 0) {
                HAGGLE_DBG("EOF reached, readlen=%u, nitems=%lu\n", ftell(fp) - fstart, nitems);
                // Yep. Close the file...
                fclose(fp);
                // ...and set the file pointer to NULL
                fp = NULL;
                return readLen;
        } else {
                //HAGGLE_DBG("Read %lu bytes data from file\n", fstop - fstart);
		
                // Was there an error?
                if (nitems != 1) {
                        // Check what kind:
                        if (ferror(fp)) {
                                HAGGLE_ERR("Tried to fill the buffer but failed, nitems=%lu\n", nitems);
                                return -1;
                        } else if (feof(fp)) {
                                HAGGLE_DBG("ERROR: End of file reached, readlen=%u, bytes left=%ld\n", fstop - fstart, bytes_left);
				
                                // Close file stream here and set to NULL.
                                // Next time this function is called readLen will be 0
                                // and returned, indicating end of file
                                fclose(fp);
                                fp = NULL;
                                return readLen;
                        }
                        HAGGLE_ERR("Undefined read error\n");
                        return -1;
                }
        }
	
        // Return how many bytes were read:
        return readLen;
}
void DataObject::setCreateTime(Timeval t)
{
        if (!metadata)
                return;
        
        createTime = t;
	has_CreateTime = true;

        metadata->setParameter(DATAOBJECT_CREATE_TIME_PARAM, createTime.getAsString());

        calcId();
}

bool DataObject::addAttribute(const Attribute& a)
{
	if (hasAttribute(a))
		return false;
	
        bool ret = attrs.add(a);
       
        calcId();

        return ret;
}

bool DataObject::addAttribute(const string name, const string value, const unsigned long weight)
{
        Attribute attr(name, value, weight);
        
        return addAttribute(attr);
}

size_t DataObject::removeAttribute(const Attribute& a)
{
        size_t n = attrs.erase(a);

        if (n > 0)
                calcId();

        return n;
}

size_t DataObject::removeAttribute(const string name, const string value)
{
        size_t n;

        if (value == "*")
                n = attrs.erase(name);
        else {
                Attribute attr(name, value);
                n = attrs.erase(attr);
        }
        
        if (n > 0)
                calcId();

        return n;
}

const Attribute *DataObject::getAttribute(const string name, const string value, const unsigned int n) const 
{
        Attribute a(name, value, n);
        Attributes::const_iterator it = attrs.find(a);

        if (it == attrs.end())
                return NULL;

        return &(*it).second;
} 

const Attributes *DataObject::getAttributes() const 
{
        return &attrs;
} 

bool DataObject::hasAttribute(const Attribute &a) const
{
	return getAttribute(a.getName(), a.getValue(), a.getWeight()) != NULL;
}

DataObject::DataState_t DataObject::verifyData()
{
        SHA_CTX ctx;
        FILE *fp;
        unsigned char *data;
        unsigned char digest[SHA_DIGEST_LENGTH];
        bool success = false;
        size_t read_bytes;
			
	if (dataLen == 0)
		return DATA_STATE_NO_DATA;

        // Is there a data hash
        if (!hasDataHash)
		return DATA_STATE_NOT_VERIFIED;
      
	if (dataState == DATA_STATE_VERIFIED_OK || dataState == DATA_STATE_VERIFIED_BAD)
		return dataState;

	dataState = DATA_STATE_VERIFIED_BAD;

        // Allocate a data buffer:
        data = (unsigned char *) malloc(4096);

	if (data == NULL)
                return dataState;

        // Open the file:
        fp = fopen(filepath.c_str(), "rb");

	if (fp == NULL) {
		HAGGLE_ERR("Could not open file %s\n", filepath.c_str());
                return dataState;
	}
        // Initialize the SHA1 hash context
        SHA1_Init(&ctx);
					
        // Go through the file until there is no more, or there was
        // an error:
        while (!(feof(fp) || ferror(fp))) {
                // Read up to 4096 bytes more:
                read_bytes = fread(data, 1, 4096, fp);
                // Insert them into the hash:
                SHA1_Update(&ctx, data, read_bytes);
        }
        // Was there an error?
        if (!ferror(fp)) {
                // No? Finalize the hash:
                SHA1_Final(digest, &ctx);
					
                // Flag success:
                success = true;
        }
        // Close the file
        fclose(fp);
        // Free the data buffer
        free(data);

        // Did we succeed above?
	if (!success)
		return dataState;
                
        // Yes? Check the hash:
        if (memcmp(dataHash, digest, SHA_DIGEST_LENGTH) != 0) {
                // Hashes not equal. Fail.
		HAGGLE_ERR("Verification failed: The data hash is not the same as the one in the data object\n");
                return dataState;
        }

	dataState = DATA_STATE_VERIFIED_OK;

        return dataState;
}

int DataObject::parseMetadata()
{
        const char *pval;

        if (!metadata)
                return -1;

        // Check persistency
        pval = metadata->getParameter(DATAOBJECT_PERSISTENT_PARAM);
        
        if (pval) {
                persistent = (strcmp(pval, "no") != 0);
        }

        // Check create time
	pval = metadata->getParameter(DATAOBJECT_CREATE_TIME_PARAM);

	if (pval) {
		Timeval ct(pval);
		createTime = ct;
		has_CreateTime = true;
	}
	
	Metadata *sm = metadata->getMetadata(DATAOBJECT_METADATA_SIGNATURE);
	
	if (sm) {
		base64_decode_context ctx;
		
		base64_decode_ctx_init(&ctx);
		
		if (signature) {
			free(signature);
			signature = NULL;
			signature_len = 0;
		}
		signee = sm->getParameter(DATAOBJECT_METADATA_SIGNATURE_SIGNEE_PARAM);
		base64_decode_alloc(&ctx, sm->getContent().c_str(), sm->getContent().length(), (char **)&signature, &signature_len);
		signatureStatus = DATAOBJECT_SIGNATURE_UNVERIFIED;
	}
	
        // Parse the "Data" tag if it exists
        Metadata *dm = metadata->getMetadata(DATAOBJECT_METADATA_DATA);

        if (dm) {
                // Check the data length
                pval = dm->getParameter(DATAOBJECT_METADATA_DATA_DATALEN_PARAM);
                
                if (pval)
                        setDataLen(strtoul(pval, NULL, 10));

                // Check optional file metadata
                Metadata *m = dm->getMetadata(DATAOBJECT_METADATA_DATA_FILENAME);

                if (m) 
                        setFileName(m->getContent());
                
		m = dm->getMetadata(DATAOBJECT_METADATA_DATA_FILEPATH);
                
                if (m) {
                        filepath = m->getContent();

                        HAGGLE_DBG("Data object has file: %s\n", filepath.c_str());
		
                        /*
                          The fopen() gets the file size of the file that is given in the
                          metadata. This really only applies to locally generated data
                          objects that are receieved from applications. In this case, the
                          payload will not arrive over the socket from the application, but
                          rather the local file is being pointed to by the file attribute in
                          the metadata. The file path and file size is in this case read
                          here with the fopen() call.

                          If the data object arrives from another node (over, e.g., the
                          network) the payload will arrive as part of a byte stream, back-
                          to-back with the metadata header. In that case, this call will
                          fail when the metadata attributes are checked here. This is
                          perfectly fine, since the file does not exists yet and is
                          currently being put.
                        */
                        FILE *fp = fopen(filepath.c_str(), "rb");

                        if (fp) {
                                // Find file size:
                                fseek(fp, 0L, SEEK_END);
                                setDataLen(ftell(fp));
                                fclose(fp);
                                HAGGLE_DBG("Size of file \'%s\' is %lu\n", filepath.c_str(), dataLen);

				dataState = DATA_STATE_NOT_VERIFIED;
                        } else {
                                HAGGLE_DBG("File \'%s\' does not exist\n", filepath.c_str());
                        }

                        // Hmm, in Windows we might need to look for a backslash instead
#ifdef OS_WINDOWS
                        filename = filepath.substr(filepath.find_last_of('\\') + 1);
#else 
                        filename = filepath.substr(filepath.find_last_of('/') + 1);
#endif
                        HAGGLE_DBG("File name is %s\n", filename.c_str());

                        setFileName(filename);

                        // Remove filepath from the metadata since it is only valid locally
                        if (!dm->removeMetadata(DATAOBJECT_METADATA_DATA_FILEPATH)) {
                                HAGGLE_ERR("Could not remove filepath metadata\n");
                        }                                
                }
		
		// Check if there is a hash
		m = dm->getMetadata(DATAOBJECT_METADATA_DATA_FILEHASH);

		if (m) {
			base64_decode_context ctx;
			size_t len = SHA_DIGEST_LENGTH;
			base64_decode_ctx_init(&ctx);

			if (base64_decode(&ctx, m->getContent().c_str(), m->getContent().length(), (char *)dataHash, &len)) {
				HAGGLE_DBG("Data object has data hash=%s\n", m->getContent().c_str());
				hasDataHash = true;
				dataState = DATA_STATE_NOT_VERIFIED;
			}
		}
        }
        
        // Parse attributes
        Metadata *mattr = metadata->getMetadata(DATAOBJECT_ATTRIBUTE_NAME);
        
        while (mattr) {
                const char *attrName = mattr->getParameter(DATAOBJECT_ATTRIBUTE_NAME_PARAM);
                const char *weightStr = mattr->getParameter(DATAOBJECT_ATTRIBUTE_WEIGHT_PARAM);
                unsigned long weight = weightStr ? strtoul(weightStr, NULL, 10) : 1;

                Attribute a(attrName, mattr->getContent(), weight);

                if (a.getName() == NODE_DESC_ATTR)
                        isNodeDesc = true;
                        
                if (!hasAttribute(a))
			attrs.add(a);
		
                mattr = metadata->getNextMetadata();
        }
        return calcId();
}
/*
  We base the unique ID of a data object on its attributes and create
  time. This means we can add other metadata to the header (e.g., for
  piggy-backing), without making the data object "different" in terms
  of what it represents to applications. Ideally, only the attributes
  and perhaps the hash of the payload (e.g., the associated data file)
  should define the object, so that two identically pieces of content
  that are published by different sources are actually seen as the
  same data objects -- in a true data-centric fashion.
 */
int DataObject::calcId()
{
        SHA_CTX ctxt;  
        
        SHA1_Init(&ctxt);
		
        for (Attributes::iterator it = attrs.begin(); it != attrs.end(); it++) {
                // Insert the name of the attribute into the hash
                SHA1_Update(&ctxt, (*it).second.getName().c_str(), 
                            (*it).second.getName().length());
		
                // Insert the value of the attribute into the hash
                SHA1_Update(&ctxt, (*it).second.getValue().c_str(), 
                            (*it).second.getValue().length());
                
                // Insert the weight of the attribute into the hash
                unsigned long w = htonl((*it).second.getWeight());
                SHA1_Update(&ctxt, (unsigned char *) &w, sizeof(w));
        }
        
	// If this data object has a create time:
	if (has_CreateTime) {
		// Add the create time to make sure the id is unique:
		SHA1_Update(&ctxt, (unsigned char *)createTime.getAsString().c_str(), createTime.getAsString().length());
	}

	/*
	 If the data object has associated data, we add the data's file hash.
	 If the data is a file, but there is no hash, then we instead use
	 the filename and data length.
	*/ 
	if (hasDataHash) {
		SHA1_Update(&ctxt, dataHash, SHA_DIGEST_LENGTH);
	} else if (filename.length() > 0 && dataLen > 0) {
		SHA1_Update(&ctxt, filename.c_str(), filename.length());
		SHA1_Update(&ctxt, &dataLen, sizeof(dataLen));
	}
	// Create the final hash value:
        SHA1_Final(this->id, &ctxt);

        // Also save as a string:
        calcIdStr();
	
	return 0;
}

void DataObject::calcIdStr()
{
        int len = 0;

        // Generate a readable string of the Id
        for (int i = 0; i < DATAOBJECT_ID_LEN; i++) {
                len += sprintf(idStr + len, "%02x", id[i] & 0xff);
        }
}

bool operator<(const DataObject & a, const DataObject & b)
{
        return 0;
}

bool operator==(const DataObject&a, const DataObject&b)
{
        return strcmp(a.getIdStr(), b.getIdStr()) == 0;
}

/*
  NOTE: Currently, the metadata is recreated/updated (at least
  partly), every time we do a getMetadata(). This is obviously not
  very efficient, but something we have to live with if we want to
  have some metadata that the data object understands and at the same
  time allow others to add their own metadata to the data object (for
  example, the "Node" tag added by the NodeManager).

  Ideally, the data object would only have one internal
  representation, and then that state is converted once to metadata at
  the time the data object goes onto the wire.
 */
Metadata *DataObject::getMetadata()
{
        return toMetadata();
}

Metadata *DataObject::toMetadata()
{
	if (!metadata)
		return NULL;
	
	metadata->setParameter(DATAOBJECT_PERSISTENT_PARAM, persistent ? "yes" : "no");
	
        // Create/update "Data" part of data object
        if (dataLen && filename.length() != 0) {
                char dataLenStr[20];
                Metadata *md = getOrCreateDataMetadata();

                if (!md)
                        return NULL;
                
                snprintf(dataLenStr, 20, "%lu", (unsigned long)dataLen);
                md->setParameter(DATAOBJECT_METADATA_DATA_DATALEN_PARAM, dataLenStr);

                /* Only add file path for data objects going to local applications. */
                if (isForLocalApp) {
                        Metadata *fpm = md->getMetadata(DATAOBJECT_METADATA_DATA_FILEPATH);
                        
                        if (fpm) {
                                fpm->setContent(filepath);
                        } else {
                                md->addMetadata(DATAOBJECT_METADATA_DATA_FILEPATH, filepath);
                        }
                }
                Metadata *fnm = md->getMetadata(DATAOBJECT_METADATA_DATA_FILENAME);
                
                if (fnm) {
                        fnm->setContent(filename);
                } else {
                        md->addMetadata(DATAOBJECT_METADATA_DATA_FILENAME, filename);
                }       
        }

	if (hasDataHash) {
		char base64_hash[BASE64_LENGTH(SHA_DIGEST_LENGTH) + 1];
		memset(base64_hash, '\0', sizeof(base64_hash));

                Metadata *md = getOrCreateDataMetadata();

		if (!md)
			return NULL;

                Metadata *fhm = md->getMetadata(DATAOBJECT_METADATA_DATA_FILEHASH);
                
                base64_encode((char *)dataHash, SHA_DIGEST_LENGTH, base64_hash, sizeof(base64_hash));

                if (fhm) {
                        fhm->setContent(base64_hash);
                } else {
                        md->addMetadata(DATAOBJECT_METADATA_DATA_FILEHASH, base64_hash);
                }
	}
	
	if (signature && signature_len) {
		Metadata *ms;
		char *base64_signature = NULL;

		if (base64_encode_alloc((char *)signature, signature_len, &base64_signature) > 0) {

			ms = metadata->getMetadata(DATAOBJECT_METADATA_SIGNATURE);

			if (ms) {
				ms->setContent(base64_signature);
			} else {
				ms = metadata->addMetadata(DATAOBJECT_METADATA_SIGNATURE, base64_signature);
			}

			if (ms) {
				ms->setParameter(DATAOBJECT_METADATA_SIGNATURE_SIGNEE_PARAM, signee.c_str());
			}
			free(base64_signature);
		}
	}
        // Sync attributes with metadata by first deleting the existing ones in the
        // metadata object and then adding the ones in our attributes container
        metadata->removeMetadata(DATAOBJECT_ATTRIBUTE_NAME);
        
        // Add attributes:
        for (Attributes::const_iterator it = attrs.begin(); it != attrs.end(); it++) {
                Metadata *mattr = metadata->addMetadata(DATAOBJECT_ATTRIBUTE_NAME, (*it).second.getValue());
              
                if (mattr) {
                        mattr->setParameter(DATAOBJECT_ATTRIBUTE_NAME_PARAM, (*it).second.getName());  
                        if ((*it).second.getWeight() != 1) {
				char weight[20];
				snprintf(weight, 20, "%lu", (*it).second.getWeight());
                                mattr->setParameter(DATAOBJECT_ATTRIBUTE_WEIGHT_PARAM, weight);
			}
                }      
        }
        return metadata;
}

ssize_t DataObject::getRawMetadata(char *raw, size_t len)
{
        if (!toMetadata())
                return -1;

        return metadata->getRaw(raw, len);
} 

bool DataObject::getRawMetadataAlloc(char **raw, size_t *len)
{
        if (!toMetadata())
                return false;

        return metadata->getRawAlloc(raw, len);
} 

bool lt_dataobj_p::operator() (const DataObject * _int1, const DataObject * _int2) const
{
        return 0;
}

bool cmp_dataobj(DataObject * o1, DataObject * o2)
{
        return lt_dataobj_p()(o1, o2);
}
