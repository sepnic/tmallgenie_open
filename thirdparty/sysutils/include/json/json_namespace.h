/*
 * Copyright (c) 2018-2022 Qinglong<sysu.zqlong@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __SYSUTILS_JSON_NAMESPACE_H__
#define __SYSUTILS_JSON_NAMESPACE_H__

#define SYSUTILS_JSON_PREFIX           sysutils
#define SYSUTILS_JSON_STATCC1(x,y,z)   SYSUTILS_JSON_STATCC2(x,y,z)
#define SYSUTILS_JSON_STATCC2(x,y,z)   x##y##z

#ifdef SYSUTILS_JSON_PREFIX
#define SYSUTILS_JSON_NAMESPACE(func)  SYSUTILS_JSON_STATCC1(SYSUTILS_JSON_PREFIX, _, func)
#else
#define SYSUTILS_JSON_NAMESPACE(func)  func
#endif

// cJSON_Utils.h
#define cJSONUtils_GetPointer                       SYSUTILS_JSON_NAMESPACE(cJSONUtils_GetPointer)
#define cJSONUtils_GetPointerCaseSensitive          SYSUTILS_JSON_NAMESPACE(cJSONUtils_GetPointerCaseSensitive)
#define cJSONUtils_GeneratePatches                  SYSUTILS_JSON_NAMESPACE(cJSONUtils_GeneratePatches)
#define cJSONUtils_GeneratePatchesCaseSensitive     SYSUTILS_JSON_NAMESPACE(cJSONUtils_GeneratePatchesCaseSensitive)
#define cJSONUtils_AddPatchToArray                  SYSUTILS_JSON_NAMESPACE(cJSONUtils_AddPatchToArray)
#define cJSONUtils_ApplyPatches                     SYSUTILS_JSON_NAMESPACE(cJSONUtils_ApplyPatches)
#define cJSONUtils_ApplyPatchesCaseSensitive        SYSUTILS_JSON_NAMESPACE(cJSONUtils_ApplyPatchesCaseSensitive)
#define cJSONUtils_MergePatch                       SYSUTILS_JSON_NAMESPACE(cJSONUtils_MergePatch)
#define cJSONUtils_MergePatchCaseSensitive          SYSUTILS_JSON_NAMESPACE(cJSONUtils_MergePatchCaseSensitive)
#define cJSONUtils_GenerateMergePatch               SYSUTILS_JSON_NAMESPACE(cJSONUtils_GenerateMergePatch)
#define cJSONUtils_GenerateMergePatchCaseSensitive  SYSUTILS_JSON_NAMESPACE(cJSONUtils_GenerateMergePatchCaseSensitive)
#define cJSONUtils_FindPointerFromObjectTo          SYSUTILS_JSON_NAMESPACE(cJSONUtils_FindPointerFromObjectTo)
#define cJSONUtils_SortObject                       SYSUTILS_JSON_NAMESPACE(cJSONUtils_SortObject)
#define cJSONUtils_SortObjectCaseSensitive          SYSUTILS_JSON_NAMESPACE(cJSONUtils_SortObjectCaseSensitive)

// cJSON.h
#define cJSON_Version                               SYSUTILS_JSON_NAMESPACE(cJSON_Version)
#define cJSON_InitHooks                             SYSUTILS_JSON_NAMESPACE(cJSON_InitHooks)
#define cJSON_Parse                                 SYSUTILS_JSON_NAMESPACE(cJSON_Parse)
#define cJSON_ParseWithLength                       SYSUTILS_JSON_NAMESPACE(cJSON_ParseWithLength)
#define cJSON_ParseWithOpts                         SYSUTILS_JSON_NAMESPACE(cJSON_ParseWithOpts)
#define cJSON_ParseWithLengthOpts                   SYSUTILS_JSON_NAMESPACE(cJSON_ParseWithLengthOpts)
#define cJSON_Print                                 SYSUTILS_JSON_NAMESPACE(cJSON_Print)
#define cJSON_PrintUnformatted                      SYSUTILS_JSON_NAMESPACE(cJSON_PrintUnformatted)
#define cJSON_PrintBuffered                         SYSUTILS_JSON_NAMESPACE(cJSON_PrintBuffered)
#define cJSON_PrintPreallocated                     SYSUTILS_JSON_NAMESPACE(cJSON_PrintPreallocated)
#define cJSON_Delete                                SYSUTILS_JSON_NAMESPACE(cJSON_Delete)
#define cJSON_GetArraySize                          SYSUTILS_JSON_NAMESPACE(cJSON_GetArraySize)
#define cJSON_GetArrayItem                          SYSUTILS_JSON_NAMESPACE(cJSON_GetArrayItem)
#define cJSON_GetObjectItem                         SYSUTILS_JSON_NAMESPACE(cJSON_GetObjectItem)
#define cJSON_GetObjectItemCaseSensitive            SYSUTILS_JSON_NAMESPACE(cJSON_GetObjectItemCaseSensitive)
#define cJSON_HasObjectItem                         SYSUTILS_JSON_NAMESPACE(cJSON_HasObjectItem)
#define cJSON_GetErrorPtr                           SYSUTILS_JSON_NAMESPACE(cJSON_GetErrorPtr)
#define cJSON_GetStringValue                        SYSUTILS_JSON_NAMESPACE(cJSON_GetStringValue)
#define cJSON_GetNumberValue                        SYSUTILS_JSON_NAMESPACE(cJSON_GetNumberValue)
#define cJSON_IsInvalid                             SYSUTILS_JSON_NAMESPACE(cJSON_IsInvalid)
#define cJSON_IsFalse                               SYSUTILS_JSON_NAMESPACE(cJSON_IsFalse)
#define cJSON_IsTrue                                SYSUTILS_JSON_NAMESPACE(cJSON_IsTrue)
#define cJSON_IsBool                                SYSUTILS_JSON_NAMESPACE(cJSON_IsBool)
#define cJSON_IsNull                                SYSUTILS_JSON_NAMESPACE(cJSON_IsNull)
#define cJSON_IsNumber                              SYSUTILS_JSON_NAMESPACE(cJSON_IsNumber)
#define cJSON_IsString                              SYSUTILS_JSON_NAMESPACE(cJSON_IsString)
#define cJSON_IsArray                               SYSUTILS_JSON_NAMESPACE(cJSON_IsArray)
#define cJSON_IsObject                              SYSUTILS_JSON_NAMESPACE(cJSON_IsObject)
#define cJSON_IsRaw                                 SYSUTILS_JSON_NAMESPACE(cJSON_IsRaw)
#define cJSON_CreateNull                            SYSUTILS_JSON_NAMESPACE(cJSON_CreateNull)
#define cJSON_CreateTrue                            SYSUTILS_JSON_NAMESPACE(cJSON_CreateTrue)
#define cJSON_CreateFalse                           SYSUTILS_JSON_NAMESPACE(cJSON_CreateFalse)
#define cJSON_CreateBool                            SYSUTILS_JSON_NAMESPACE(cJSON_CreateBool)
#define cJSON_CreateNumber                          SYSUTILS_JSON_NAMESPACE(cJSON_CreateNumber)
#define cJSON_CreateString                          SYSUTILS_JSON_NAMESPACE(cJSON_CreateString)
#define cJSON_CreateRaw                             SYSUTILS_JSON_NAMESPACE(cJSON_CreateRaw)
#define cJSON_CreateArray                           SYSUTILS_JSON_NAMESPACE(cJSON_CreateArray)
#define cJSON_CreateObject                          SYSUTILS_JSON_NAMESPACE(cJSON_CreateObject)
#define cJSON_CreateStringReference                 SYSUTILS_JSON_NAMESPACE(cJSON_CreateStringReference)
#define cJSON_CreateObjectReference                 SYSUTILS_JSON_NAMESPACE(cJSON_CreateObjectReference)
#define cJSON_CreateArrayReference                  SYSUTILS_JSON_NAMESPACE(cJSON_CreateArrayReference)
#define cJSON_CreateIntArray                        SYSUTILS_JSON_NAMESPACE(cJSON_CreateIntArray)
#define cJSON_CreateFloatArray                      SYSUTILS_JSON_NAMESPACE(cJSON_CreateFloatArray)
#define cJSON_CreateDoubleArray                     SYSUTILS_JSON_NAMESPACE(cJSON_CreateDoubleArray)
#define cJSON_CreateStringArray                     SYSUTILS_JSON_NAMESPACE(cJSON_CreateStringArray)
#define cJSON_AddItemToArray                        SYSUTILS_JSON_NAMESPACE(cJSON_AddItemToArray)
#define cJSON_AddItemToObject                       SYSUTILS_JSON_NAMESPACE(cJSON_AddItemToObject)
#define cJSON_AddItemToObjectCS                     SYSUTILS_JSON_NAMESPACE(cJSON_AddItemToObjectCS)
#define cJSON_AddItemReferenceToArray               SYSUTILS_JSON_NAMESPACE(cJSON_AddItemReferenceToArray)
#define cJSON_AddItemReferenceToObject              SYSUTILS_JSON_NAMESPACE(cJSON_AddItemReferenceToObject)
#define cJSON_DetachItemViaPointer                  SYSUTILS_JSON_NAMESPACE(cJSON_DetachItemViaPointer)
#define cJSON_DetachItemFromArray                   SYSUTILS_JSON_NAMESPACE(cJSON_DetachItemFromArray)
#define cJSON_DeleteItemFromArray                   SYSUTILS_JSON_NAMESPACE(cJSON_DeleteItemFromArray)
#define cJSON_DetachItemFromObject                  SYSUTILS_JSON_NAMESPACE(cJSON_DetachItemFromObject)
#define cJSON_DetachItemFromObjectCaseSensitive     SYSUTILS_JSON_NAMESPACE(cJSON_DetachItemFromObjectCaseSensitive)
#define cJSON_DeleteItemFromObject                  SYSUTILS_JSON_NAMESPACE(cJSON_DeleteItemFromObject)
#define cJSON_DeleteItemFromObjectCaseSensitive     SYSUTILS_JSON_NAMESPACE(cJSON_DeleteItemFromObjectCaseSensitive)
#define cJSON_InsertItemInArray                     SYSUTILS_JSON_NAMESPACE(cJSON_InsertItemInArray)
#define cJSON_ReplaceItemViaPointer                 SYSUTILS_JSON_NAMESPACE(cJSON_ReplaceItemViaPointer)
#define cJSON_ReplaceItemInArray                    SYSUTILS_JSON_NAMESPACE(cJSON_ReplaceItemInArray)
#define cJSON_ReplaceItemInObject                   SYSUTILS_JSON_NAMESPACE(cJSON_ReplaceItemInObject)
#define cJSON_ReplaceItemInObjectCaseSensitive      SYSUTILS_JSON_NAMESPACE(cJSON_ReplaceItemInObjectCaseSensitive)
#define cJSON_Duplicate                             SYSUTILS_JSON_NAMESPACE(cJSON_Duplicate)
#define cJSON_Compare                               SYSUTILS_JSON_NAMESPACE(cJSON_Compare)
#define cJSON_Minify                                SYSUTILS_JSON_NAMESPACE(cJSON_Minify)
#define cJSON_AddNullToObject                       SYSUTILS_JSON_NAMESPACE(cJSON_AddNullToObject)
#define cJSON_AddTrueToObject                       SYSUTILS_JSON_NAMESPACE(cJSON_AddTrueToObject)
#define cJSON_AddFalseToObject                      SYSUTILS_JSON_NAMESPACE(cJSON_AddFalseToObject)
#define cJSON_AddBoolToObject                       SYSUTILS_JSON_NAMESPACE(cJSON_AddBoolToObject)
#define cJSON_AddNumberToObject                     SYSUTILS_JSON_NAMESPACE(cJSON_AddNumberToObject)
#define cJSON_AddStringToObject                     SYSUTILS_JSON_NAMESPACE(cJSON_AddStringToObject)
#define cJSON_AddRawToObject                        SYSUTILS_JSON_NAMESPACE(cJSON_AddRawToObject)
#define cJSON_AddObjectToObject                     SYSUTILS_JSON_NAMESPACE(cJSON_AddObjectToObject)
#define cJSON_AddArrayToObject                      SYSUTILS_JSON_NAMESPACE(cJSON_AddArrayToObject)
#define cJSON_SetNumberHelper                       SYSUTILS_JSON_NAMESPACE(cJSON_SetNumberHelper)
#define cJSON_SetValuestring                        SYSUTILS_JSON_NAMESPACE(cJSON_SetValuestring)
#define cJSON_malloc                                SYSUTILS_JSON_NAMESPACE(cJSON_malloc)
#define cJSON_free                                  SYSUTILS_JSON_NAMESPACE(cJSON_free)

#endif /* __SYSUTILS_HTTPCLIENT_NAMESPACE_H__ */
