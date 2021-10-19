/*
 * Copyright (c) 2019 SK Telecom Co., Ltd. All rights reserved.
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

#include "capability/asr_interface.hh"

namespace NuguCapability {

void IASRListener::onState(ASRState state, const std::string& dialog_id) { }
void IASRListener::onState(ASRState state, const std::string& dialog_id, ASRInitiator initiator) { }
void IASRListener::onNone(const std::string& dialog_id) { }
void IASRListener::onPartial(const std::string& text, const std::string& dialog_id) { }
void IASRListener::onComplete(const std::string& text, const std::string& dialog_id) { }
void IASRListener::onError(ASRError error, const std::string& dialog_id, bool listen_timeout_fail_beep) { }
void IASRListener::onCancel(const std::string& dialog_id) { }

} // NuguCapability
