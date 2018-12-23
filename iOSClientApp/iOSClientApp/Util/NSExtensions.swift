//
//  NSExtensions.swift
//
//  Created by TCCODER on 30/10/17.
//  Copyright © 2018 Topcoder. All rights reserved.
//

import UIKit
import SwiftyJSON

// MARK: - class name
extension NSObject {
 
    /// class name
    static var className: String {
        return String(describing: self)
    }
    
    /// class name
    var className: String {
        return String(describing: type(of: self))
    }
    
}

// MARK: - string extensions
extension String {
    
    /**
     Get string without spaces at the end and at the start.
     
     - returns: trimmed string
     */
    func trim() -> String {
        return self.trimmingCharacters(in: CharacterSet.whitespacesAndNewlines)
    }
    
    /// localized string
    var localized: String {
        return NSLocalizedString(self, comment: "")
    }
    
}

// MARK: - date formatting
extension Date {
    
    /// default date formatter
    static let DefaultFormatter: DateFormatter = {
        let df = DateFormatter()
        df.dateFormat = "MM-dd-yyyy' 'HH:mm"
        return df
    }()
    
    /// default formatted string
    var defaultFormat: String {
        return Date.DefaultFormatter.string(from: self)
    }
    
}

// MARK: - dictionary extension
extension Dictionary {
    
    /// Maps only the values of receiver and returns as a new dictionary
    func mapValues<T>(_ transform: (Key, Value)->T) -> Dictionary<Key,T> {
        var resultDict = [Key: T]()
        for (k, v) in self {
            resultDict[k] = transform(k, v)
        }
        return resultDict
    }
}

// MARK: - dictionary extension
extension Dictionary where Value: Optionable {
    
    /// flattens dictionary values
    ///
    /// - Returns: flattened dictionary
    func flattenValues() -> [Key: Value.Wrapped] {
        var result = [Key: Value.Wrapped]()
        for (k, v) in self {
            if let v = v.value {
                result[k] = v
            }
        }
        return result
    }
    
}
