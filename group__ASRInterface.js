var group__ASRInterface =
[
    [ "_ASRAttribute", "structNuguCapability_1_1__ASRAttribute.html", [
      [ "model_path", "structNuguCapability_1_1__ASRAttribute.html#ae52cec3ec0541489cb35534e7dc83f5d", null ],
      [ "epd_type", "structNuguCapability_1_1__ASRAttribute.html#acef924f58318d143127d86b3c28b6798", null ],
      [ "asr_encoding", "structNuguCapability_1_1__ASRAttribute.html#aca8cff1b929878b6517145826359d95b", null ],
      [ "response_timeout", "structNuguCapability_1_1__ASRAttribute.html#aa9697fbb4821881636a853ff1e894b91", null ]
    ] ],
    [ "IASRHandler", "classNuguCapability_1_1IASRHandler.html", [
      [ "AsrRecognizeCallback", "classNuguCapability_1_1IASRHandler.html#ad49e6bf6b6f66c6ab0ed3dcfbede937f", null ],
      [ "~IASRHandler", "classNuguCapability_1_1IASRHandler.html#a41f13c7acf4e08a3400373c2c5438958", null ],
      [ "startRecognition", "classNuguCapability_1_1IASRHandler.html#aeb6cfdd1d86a2031dca35a3eb9e51b63", null ],
      [ "startRecognition", "classNuguCapability_1_1IASRHandler.html#ad1c1f4d6b8c9c0feb5090e06fcf5909a", null ],
      [ "stopRecognition", "classNuguCapability_1_1IASRHandler.html#a26bdfa437e889529c32489d0493441d8", null ],
      [ "addListener", "classNuguCapability_1_1IASRHandler.html#a36f354f3d51036495d0686723c55ce8c", null ],
      [ "removeListener", "classNuguCapability_1_1IASRHandler.html#ad4d340a3517a6f9d9ae6dba03574617e", null ],
      [ "setAttribute", "classNuguCapability_1_1IASRHandler.html#a90ba983d4e72307c739b59d7c870b2de", null ],
      [ "setEpdAttribute", "classNuguCapability_1_1IASRHandler.html#afc93c6551640bb44cbd0fc5565f01657", null ],
      [ "getEpdAttribute", "classNuguCapability_1_1IASRHandler.html#aa3c2e35375d8524d75ac74bba98247ed", null ]
    ] ],
    [ "IASRListener", "classNuguCapability_1_1IASRListener.html", [
      [ "~IASRListener", "classNuguCapability_1_1IASRListener.html#a0ed3468bba18f9c4a597061fe2ca3976", null ],
      [ "onState", "classNuguCapability_1_1IASRListener.html#ae517b3332c499a970be81f33d5f5e442", null ],
      [ "onNone", "classNuguCapability_1_1IASRListener.html#aba55f5cb3892f44a2c9c32ba70bd3723", null ],
      [ "onPartial", "classNuguCapability_1_1IASRListener.html#a2395171f2c033b205f880f14b9a6646f", null ],
      [ "onComplete", "classNuguCapability_1_1IASRListener.html#acd823bdf21a98bc80ed20207cfc88999", null ],
      [ "onError", "classNuguCapability_1_1IASRListener.html#a14209e11ff08651e11b5d5c0cc2c323b", null ],
      [ "onCancel", "classNuguCapability_1_1IASRListener.html#afd6df670314542e5bce36444921258f6", null ]
    ] ],
    [ "ASRAttribute", "group__ASRInterface.html#ga30efbf9d5ab40cdffdf64a665d412d3c", null ],
    [ "ASRState", "group__ASRInterface.html#gafe4f48f063bafec608e5060090a9543b", [
      [ "IDLE", "group__ASRInterface.html#ggafe4f48f063bafec608e5060090a9543baa5daf7f2ebbba4975d61dab1c40188c7", null ],
      [ "EXPECTING_SPEECH", "group__ASRInterface.html#ggafe4f48f063bafec608e5060090a9543ba2ad686da9d27bb3646616a1620173f83", null ],
      [ "LISTENING", "group__ASRInterface.html#ggafe4f48f063bafec608e5060090a9543bac0ff938e396e72c225bd66562b80a77e", null ],
      [ "RECOGNIZING", "group__ASRInterface.html#ggafe4f48f063bafec608e5060090a9543ba437f3cbaf966fe37c60ee219ecb23576", null ],
      [ "BUSY", "group__ASRInterface.html#ggafe4f48f063bafec608e5060090a9543ba802706a9238e2928077f97736854bad4", null ]
    ] ],
    [ "ASRInitiator", "group__ASRInterface.html#ga0dd27d783b014cca3e5aca7510b36f8a", [
      [ "WAKE_UP_WORD", "group__ASRInterface.html#gga0dd27d783b014cca3e5aca7510b36f8aa646e5558f4e20f45fbd651b653f8dbc3", null ],
      [ "PRESS_AND_HOLD", "group__ASRInterface.html#gga0dd27d783b014cca3e5aca7510b36f8aa56d48be09b7b10cc583583453bc87a9b", null ],
      [ "TAP", "group__ASRInterface.html#gga0dd27d783b014cca3e5aca7510b36f8aafcd6420c60a17418a6de745d6546d966", null ],
      [ "EXPECT_SPEECH", "group__ASRInterface.html#gga0dd27d783b014cca3e5aca7510b36f8aae93ac2a078fa28db0eb331bf67e35021", null ],
      [ "EARSET", "group__ASRInterface.html#gga0dd27d783b014cca3e5aca7510b36f8aa6f9af525f01e8f64a9d550cfd8484613", null ]
    ] ],
    [ "ASRError", "group__ASRInterface.html#ga9aa11256d9ce8a3aa14ac9a24e1d8e21", [
      [ "RESPONSE_TIMEOUT", "group__ASRInterface.html#gga9aa11256d9ce8a3aa14ac9a24e1d8e21a0bc1f3f491e9717b6ff83103a014d496", null ],
      [ "LISTEN_TIMEOUT", "group__ASRInterface.html#gga9aa11256d9ce8a3aa14ac9a24e1d8e21a54a62829eeacbac64ad20c96ade3b58d", null ],
      [ "LISTEN_FAILED", "group__ASRInterface.html#gga9aa11256d9ce8a3aa14ac9a24e1d8e21a25b5127a7de62d625277c315c505f18f", null ],
      [ "RECOGNIZE_ERROR", "group__ASRInterface.html#gga9aa11256d9ce8a3aa14ac9a24e1d8e21ab7bc01e941b272a54a236e1303aed2a1", null ],
      [ "UNKNOWN", "group__ASRInterface.html#gga9aa11256d9ce8a3aa14ac9a24e1d8e21a696b031073e74bf2cb98e5ef201d4aa3", null ]
    ] ]
];