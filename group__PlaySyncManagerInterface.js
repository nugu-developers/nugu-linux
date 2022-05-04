var group__PlaySyncManagerInterface =
[
    [ "IPlaySyncManager", "classNuguClientKit_1_1IPlaySyncManager.html", [
      [ "PlaySyncContainer", "classNuguClientKit_1_1IPlaySyncManager.html#a8ec85a0fe21167868a7403c7bc8ccaed", null ],
      [ "PlayStacks", "classNuguClientKit_1_1IPlaySyncManager.html#aa9566efe181d6f6e00f5d33053d783dc", null ],
      [ "~IPlaySyncManager", "classNuguClientKit_1_1IPlaySyncManager.html#a9b89a8ddc144c7b04814b82e6a34102f", null ],
      [ "registerCapabilityForSync", "classNuguClientKit_1_1IPlaySyncManager.html#a67226c6cb1b77d4ed5825906b5310671", null ],
      [ "addListener", "classNuguClientKit_1_1IPlaySyncManager.html#af7e598ba1e9afd95ba6151c4d7f27bc0", null ],
      [ "removeListener", "classNuguClientKit_1_1IPlaySyncManager.html#a974da00d08b54dbf5b8a6a1d2029ed2c", null ],
      [ "prepareSync", "classNuguClientKit_1_1IPlaySyncManager.html#a975de44a9d118c2ab0a65a4427e87564", null ],
      [ "startSync", "classNuguClientKit_1_1IPlaySyncManager.html#aa7e5ed501eb0e3076097a291ef2265ca", null ],
      [ "cancelSync", "classNuguClientKit_1_1IPlaySyncManager.html#a06bb48da272814296a48e7f34a8c27cd", null ],
      [ "releaseSync", "classNuguClientKit_1_1IPlaySyncManager.html#a573fdad089aacfa994bbd4b90166fe48", null ],
      [ "releaseSyncLater", "classNuguClientKit_1_1IPlaySyncManager.html#a6336bb1ba512b093727726a9bdfa34f6", null ],
      [ "releaseSyncImmediately", "classNuguClientKit_1_1IPlaySyncManager.html#aaccede352752e59a78067863d06d216e", null ],
      [ "releaseSyncUnconditionally", "classNuguClientKit_1_1IPlaySyncManager.html#a5fc1db8fc8bc6e34d831878c1fa9a8bc", null ],
      [ "postPoneRelease", "classNuguClientKit_1_1IPlaySyncManager.html#af4ccf0a65663e311732a424441e91681", null ],
      [ "continueRelease", "classNuguClientKit_1_1IPlaySyncManager.html#a5eebf9023193a331f52709e246329465", null ],
      [ "stopHolding", "classNuguClientKit_1_1IPlaySyncManager.html#a8a9dfd81efab9b66881e31d57713d392", null ],
      [ "resetHolding", "classNuguClientKit_1_1IPlaySyncManager.html#aac5186353e255533370547434447791f", null ],
      [ "clearHolding", "classNuguClientKit_1_1IPlaySyncManager.html#a1a0e795464aba59343d48f35ce7ad25c", null ],
      [ "restartHolding", "classNuguClientKit_1_1IPlaySyncManager.html#a38d6a7c6a9d3c381d583e8ba89db5781", null ],
      [ "clear", "classNuguClientKit_1_1IPlaySyncManager.html#aa2f2b27e5e833ced20d1bd3e3572dd94", null ],
      [ "isConditionToHandlePrevDialog", "classNuguClientKit_1_1IPlaySyncManager.html#a9b8b814ba34f90cb7a3c55b05abf8728", null ],
      [ "hasActivity", "classNuguClientKit_1_1IPlaySyncManager.html#aec1d0b6c858178b591362fff289f5473", null ],
      [ "hasNextPlayStack", "classNuguClientKit_1_1IPlaySyncManager.html#a7a690e691ce704a5fa593ed1df23f9b6", null ],
      [ "getAllPlayStackItems", "classNuguClientKit_1_1IPlaySyncManager.html#a2741b98b7ba7b565b65fd894ac7026a5", null ],
      [ "adjustPlayStackHoldTime", "classNuguClientKit_1_1IPlaySyncManager.html#a8719bfa1b4279dd1d36ce8b74e3e4082", null ],
      [ "setDefaultPlayStackHoldTime", "classNuguClientKit_1_1IPlaySyncManager.html#a57084344446796bbbb5235c0f1bfbc7b", null ],
      [ "replacePlayStack", "classNuguClientKit_1_1IPlaySyncManager.html#a8a6d7d91c2506ce80972e426eb563faa", null ]
    ] ],
    [ "IPlaySyncManagerListener", "classNuguClientKit_1_1IPlaySyncManagerListener.html", [
      [ "~IPlaySyncManagerListener", "classNuguClientKit_1_1IPlaySyncManagerListener.html#afa13820115fa84e0b55f04b48a66ef43", null ],
      [ "onSyncState", "classNuguClientKit_1_1IPlaySyncManagerListener.html#a255b59b068d4478b5ead4b6aa12c4f46", null ],
      [ "onDataChanged", "classNuguClientKit_1_1IPlaySyncManagerListener.html#a083c20c7187973da7917c5d9279804b8", null ],
      [ "onStackChanged", "classNuguClientKit_1_1IPlaySyncManagerListener.html#a8aa0d286ec7d21711644a6c8a9b0bc1b", null ]
    ] ],
    [ "PlaySyncState", "group__PlaySyncManagerInterface.html#ga5d9f5e0329e5706191b9a4682615d4d0", [
      [ "None", "group__PlaySyncManagerInterface.html#gga5d9f5e0329e5706191b9a4682615d4d0a6adf97f83acf6453d4a6a4b1070f3754", null ],
      [ "Prepared", "group__PlaySyncManagerInterface.html#gga5d9f5e0329e5706191b9a4682615d4d0a4f8ebbe84c83c694e33dfc679cf40ddb", null ],
      [ "Synced", "group__PlaySyncManagerInterface.html#gga5d9f5e0329e5706191b9a4682615d4d0a5befab0dde764b6dd8b24a34dc30afa7", null ],
      [ "Released", "group__PlaySyncManagerInterface.html#gga5d9f5e0329e5706191b9a4682615d4d0aea1e34304a5d8ffa7c9b0ed8ede4ef1a", null ],
      [ "Appending", "group__PlaySyncManagerInterface.html#gga5d9f5e0329e5706191b9a4682615d4d0a40833ea8a03550d44b5c703f72fd24a7", null ]
    ] ],
    [ "PlayStackActivity", "group__PlaySyncManagerInterface.html#ga42c294733588594b84ae16137eb054c5", [
      [ "None", "group__PlaySyncManagerInterface.html#gga42c294733588594b84ae16137eb054c5a6adf97f83acf6453d4a6a4b1070f3754", null ],
      [ "Alert", "group__PlaySyncManagerInterface.html#gga42c294733588594b84ae16137eb054c5ab92071d61c88498171928745ca53078b", null ],
      [ "Call", "group__PlaySyncManagerInterface.html#gga42c294733588594b84ae16137eb054c5ac3755e61202abd74da5885d2e9c9160e", null ],
      [ "TTS", "group__PlaySyncManagerInterface.html#gga42c294733588594b84ae16137eb054c5a4616606d5a8590d8c1d305d50dce2f73", null ],
      [ "Media", "group__PlaySyncManagerInterface.html#gga42c294733588594b84ae16137eb054c5a3b563524fdb17b4a86590470d40bef74", null ]
    ] ]
];