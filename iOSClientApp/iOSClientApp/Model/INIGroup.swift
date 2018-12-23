//
//  INIGroup.swift
//  iOSClientApp
//
//  Created by Nikita Rodin on 12/18/18.
//  Copyright © 2018 Nikita Rodin. All rights reserved.
//

import Foundation

/// common interface for .ini items
protocol INIItem {
    
    /// title
    var title: String { get }
    
    /// is header
    var isHeader: Bool { get }
    
}

struct INIGroup: Codable {
    
    /// fields
    let name: String
    let keyValues: [INIKeyValue]
    
}

// MARK: - INIItem
extension INIGroup: INIItem {
    
    /// title
    var title: String {
        return "[\(name)]"
    }
    
    /// is header
    var isHeader: Bool {
        return true
    }
    
}
