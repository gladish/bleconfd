//
//  BLEAPI.swift
//  iOSClientApp
//
//  Created by Nikita Rodin on 12/18/18.
//  Copyright © 2018 Nikita Rodin. All rights reserved.
//

import UIKit
import RxCocoa
import RxSwift
import SwiftyJSON

// MARK: - API methods
extension BLEAPI {
    
    // MARK: ini
    
    /// get ini groups
    ///
    /// - Returns: call observable
    func getINIGroups() -> Observable<[INIGroup]> {
        return send(request: "\(Configuration.settingsAPI)-get-all",
            parameters: [:])
            .logRequest()
            .map {
                let json = $0.json
                let text = $0.originalString
                
                // validate
                if let error = $0.json["error", "message"].string {
                    throw error
                }

                // parse groups
                let result = json["result"].dictionaryValue.map {
                    INIGroup(name: $0.key, keyValues: $0.value.dictionaryValue.map {
                        INIKeyValue(key: $0.key, value: $0.value.stringValue)
                    })
                }
                
                // restore order
                return result
                    .sorted(by: { text.range(of: "\"\($0.name)\":")!.lowerBound < text.range(of: "\"\($1.name)\":")!.lowerBound })
            }
            .requestSend()
    }
    
    /// add group
    ///
    /// - Parameter group: the group name
    /// - Returns: call observable
    func add(group: String) -> Observable<[INIGroup]> {
        return send(request: "\(Configuration.settingsAPI)-group-add",
            parameters: [
                "params": [
                    "group": group
                ]
            ])
            .logRequest()
            .requestSend()
            .validateAndFetchGroups()
    }
    
    /// edit group
    ///
    /// - Parameter group: the old group name
    /// - Parameter newName: the new group name
    /// - Returns: call observable
    func edit(group: String, newName: String) -> Observable<[INIGroup]> {
        return send(request: "\(Configuration.settingsAPI)-group-modify",
            parameters: [
                "params": [
                    "originGroup": group,
                    "newGroup": newName,
                ]
            ])
            .logRequest()
            .requestSend()
            .validateAndFetchGroups()
    }
    
    /// add/edit value
    ///
    /// - Parameter key: the key
    /// - Parameter value: the value
    /// - Parameter group: the group name
    /// - Returns: call observable
    func addOrEdit(key: String, value: String, group: String) -> Observable<[INIGroup]> {
        return send(request: "\(Configuration.settingsAPI)-set",
            parameters: [
                "params": [
                    "group": group,
                    "name": key,
                    "value": value,
                ]
            ])
            .logRequest()
            .requestSend()
            .validateAndFetchGroups()
    }
    
    /// delete group
    ///
    /// - Parameter group: the group name
    /// - Returns: call observable
    func delete(group: String) -> Observable<[INIGroup]> {
        return send(request: "\(Configuration.settingsAPI)-group-delete",
            parameters: [
                "params": [
                    "group": group
                ]
            ])
            .logRequest()
            .requestSend()
            .validateAndFetchGroups()
    }
    
    /// delete key from group
    ///
    /// - Parameter key: the key
    /// - Parameter group: the group name
    /// - Returns: call observable
    func delete(key: String, group: String) -> Observable<[INIGroup]> {
        return send(request: "\(Configuration.settingsAPI)-delete",
            parameters: [
                "params": [
                    "group": group,
                    "name": key
                ]
            ])
            .logRequest()
            .requestSend()
            .validateAndFetchGroups()
    }
    
    // MARK: Wi-Fi
    
    /// set Wi-fi
    ///
    /// - Parameters:
    ///   - ssid: SSID
    ///   - password: password
    /// - Returns: call observable
    func setWifi(ssid: String, password: String) -> Observable<Result> {
        return send(request: "\(Configuration.wifiAPI)-connect",
            parameters: [
                "wi-fi_tech": "infra",
                "discovery": [
                    "ssid": ssid
                ],
                "cred": [
                    "akm": "psk",
                    "pass": password
                ]
            ], resultKey: "result")
            .logRequest()
            .decode(Result.self)
            .observeOn(MainScheduler.instance)
            .take(2)
            .share(replay: 1)
        
    }
    
    /// gets wifi status
    ///
    /// - Returns: call observable
    func getWifiStatus() -> Observable<Result> {
        return send(request: "\(Configuration.wifiAPI)-get-status",
            parameters: [:])
            .logRequest()
            .decode(Result.self, key: "result")
            .requestSend()
    }
    
    
}

// MARK: - shortcut for REST service
extension Observable {
    
    /// wrap remote call in shareReplay & observe on main thread
    fileprivate func requestSend() -> Observable<Element> {
        return self.observeOn(MainScheduler.instance)
            .take(1)
            .share(replay: 1)
    }
    
}

// MARK: - shortcuts for response parsing
extension Observable where Element == InboxPacket {
    
    /// discard result type
    fileprivate func logRequest() -> Observable<Element> {
        return self.do(onNext: { Logger.log(.debug, "Received response for request \($0.json["id"].int ?? -1)") },
                       onError: { Logger.log(.error, $0 as? String ?? $0.localizedDescription) })
        
    }
    
    /// JSON decode
    ///
    /// - Parameter type: decodable type
    /// - Returns: decoded observable
    fileprivate func decode<T: Decodable>(_ type: T.Type, key: String? = nil) -> Observable<T> {
        return self
            .map {
                try JSONDecoder().decode(T.self, from: try (key == nil ? $0.json : $0.json[key!]).rawData())
        }
    }
    
    /// validates response and returns fresh groups if operation successful
    ///
    /// - Returns: groups observable
    fileprivate func validateAndFetchGroups() -> Observable<[INIGroup]> {
        return self.flatMapLatest {
            $0.json["error", "message"].string == nil ? BLE.api.getINIGroups()
                : Observable<[INIGroup]>.error($0.json["error", "message"].stringValue)
        }
    }
    
}
