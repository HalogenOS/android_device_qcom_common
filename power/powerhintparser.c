/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <cutils/log.h>
#include <fcntl.h>
#include <string.h>
#include <cutils/properties.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "powerhintparser.h"
#define LOG_TAG "QTI PowerHintParser"


int parsePowerhintXML() {

    xmlDocPtr doc;
    xmlNodePtr currNode;
    const char *opcode_str, *value_str, *type_str;
    int opcode = 0, value = 0, type = 0;
    int numParams = 0;
    static int hintCount;
    int fd;
    char buf[10] = {0};
    char path[39];

    /* powerhint.xml is soc_id specific */
    fd = open("/sys/devices/soc0/soc_id", O_RDONLY);
    if (fd >= 0) {
        if (read(fd, buf, sizeof(buf) - 1) == -1) {
            ALOGW("Unable to read soc_id");
        } else {
            int soc_id = atoi(buf);
            snprintf(path, MAX_LIB_PATH,"/system/etc/powerhint_soc_id_%d.xml",
                    soc_id);
        }
        close(fd);
    }

    if(access(path, F_OK) < 0) {
        path = POWERHINT_XML
        if (access(path, F_OK) < 0) {
            return -1
        }
    }

    doc = xmlReadFile(path, "UTF-8", XML_PARSE_RECOVER);
    if(!doc) {
        ALOGE("Document not parsed successfully");
        return -1;
    }

    currNode = xmlDocGetRootElement(doc);
    if(!currNode) {
        ALOGE("Empty document");
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return -1;
    }

    // Confirm the root-element of the tree
    if(xmlStrcmp(currNode->name, BAD_CAST "Powerhint")) {
        ALOGE("document of the wrong type, root node != root");
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return -1;
    }

    currNode = currNode->xmlChildrenNode;

    for(; currNode != NULL; currNode=currNode->next) {

        if(currNode->type != XML_ELEMENT_NODE)
            continue;

        xmlNodePtr node = currNode;

        if(hintCount == MAX_HINT) {
            ALOGE("Number of hints exceeded the max count of %d\n",MAX_HINT);
            break;
        }

        if(!xmlStrcmp(node->name, BAD_CAST "Hint")) {
            if(xmlHasProp(node, BAD_CAST "type")) {
               type_str = (const char*)xmlGetProp(node, BAD_CAST "type");
               if (type_str == NULL)
               {
                   ALOGE("xmlGetProp failed on type");
                   xmlFreeDoc(doc);
                   xmlCleanupParser();
                   return -1;
               }
               type = strtoul(type_str, NULL, 16);
            }

            node = node->children;
            while(node != NULL) {
                if(!xmlStrcmp(node->name, BAD_CAST "Resource")) {

                    if(xmlHasProp(node, BAD_CAST "opcode")) {
                        opcode_str  = (const char*)xmlGetProp(node, BAD_CAST "opcode");
                        if (opcode_str == NULL)
                        {
                            ALOGE("xmlGetProp failed on opcode");
                            xmlFreeDoc(doc);
                            xmlCleanupParser();
                            return -1;
                        }
                        opcode = strtol(opcode_str, NULL, 16);
                    }
                    if(xmlHasProp(node, BAD_CAST "value")) {
                        value_str = (const char*)xmlGetProp(node, BAD_CAST "value");
                        if (value_str == NULL)
                        {
                            ALOGE("xmlGetProp failed on value");
                            xmlFreeDoc(doc);
                            xmlCleanupParser();
                            return -1;
                        }
                        value = strtol(value_str, NULL, 16);
                    }
                    if(opcode > 0) {
                        if(numParams < (MAX_PARAM-1)) {
                            powerhint[hintCount].paramList[numParams++] = opcode;
                            powerhint[hintCount].paramList[numParams++] = value;
                        } else {
                            ALOGE("Maximum parameters exceeded for Hint ID %x\n",type);
                            opcode = value = 0;
                            break;
                        }
                    }

                    opcode = value = 0;
                }
                node = node->next;
            }
            powerhint[hintCount].type = type;
            powerhint[hintCount].numParams = numParams;
            numParams = 0;
        }
        hintCount++;
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}

int* getPowerhint(int hint_id, int *params) {

   int *result = NULL;

   if(!hint_id)
       return result;

    ALOGI("Powerhal hint received=%x\n",hint_id);

    if(!powerhint[0].numParams) {
       parsePowerhintXML();
    }

    for(int i = 0; i < MAX_HINT; i++) {
       if(hint_id == powerhint[i].type) {
          *params = powerhint[i].numParams;
          result = powerhint[i].paramList;
          break;
       }
    }

    ALOGI("Returning resource list with %d total resources\n",*params);
    return result;
}
