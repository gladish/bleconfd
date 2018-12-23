//
//  Configuration.swift
//
//  Created by TCCODER on 6/9/17.
//  Copyright © 2017 topcoder. All rights reserved.
//

import UIKit
import CoreBluetooth

/// A helper class to get the configuration data in the plist file.
final class Configuration: NSObject {
   
    /// data
    var dict = NSDictionary(contentsOfFile: Bundle.main.path(forResource: "Configuration", ofType: "plist")!)
    
    /// singleton
    static let sharedInstance = Configuration()

    /// log level
    static var logLevel: Int {
        return sharedInstance.dict!["logLevel"] as? Int ?? -1
    }
    
    /// RPC service UUID
    static var rpcServiceUUID: CBUUID {
        return CBUUID(string: sharedInstance.dict!["rpcServiceUUID"] as! String)
    }
    
    /// RPC inbox UUID
    static var rpcInboxUUID: CBUUID {
        return CBUUID(string: sharedInstance.dict!["rpcInboxUUID"] as! String)
    }
    
    /// polling UUID
    static var ePollUUID: CBUUID {
        return CBUUID(string: sharedInstance.dict!["ePollUUID"] as! String)
    }
    
    /// server advertised UUIDs
    static var deviceAdvertisedUUIDs: [CBUUID] {
        let string = sharedInstance.dict!["deviceAdvertisedUUID"] as? String ?? ""
        return string.trim().isEmpty ? [] : [CBUUID(string: string)]
    }
    
    /// auto pair first suitable device
    static var autoPair: Bool {
        return sharedInstance.dict!["autoPair"] as! Bool
    }
    
    /// settings API service name
    static var settingsAPI: String {
        return sharedInstance.dict!["settingsAPI"] as! String
    }
    
    /// Wi-Fi API service name
    static var wifiAPI: String {
        return sharedInstance.dict!["wifiAPI"] as! String
    }
    
    /// polling delay in s
    static var pollingDelay: TimeInterval {
        return sharedInstance.dict!["pollingDelay"] as! TimeInterval / 1000
    }
    
    /// print log to console as well
    static var logPrintConsole: Bool {
        return sharedInstance.dict!["logPrintConsole"] as? Bool ?? false
    }
    
    /// include time in log messages
    static var logIncludeTime: Bool {
        return sharedInstance.dict!["logIncludeTime"] as? Bool ?? false
    }

}
