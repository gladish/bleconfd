//
//  INIValue.swift
//  iOSClientApp
//
//  Created by Nikita Rodin on 12/18/18.
//  Copyright © 2018 Nikita Rodin. All rights reserved.
//

import Foundation

struct INIKeyValue: Codable {

    /// fields
    let key: String
    let value: String
    
}

// MARK: - INIItem
extension INIKeyValue: INIItem {
    
    /// title
    var title: String {
        return "\(key)=\(value)"
    }
    
    /// is header
    var isHeader: Bool {
        return false
    }
    
}
