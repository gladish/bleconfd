//
//  Storyboards.swift
//  iOSClientApp
//
//  Created by Nikita Rodin on 12/16/18.
//  Copyright © 2018 Nikita Rodin. All rights reserved.
//

import UIKit

/// Describes available for instantiation storyboards
enum Storyboards: String {
    
    /// storyboards
    case main = "Main",
        home = "Home",
        log = "Log",
        wifi = "WiFi",
        ini = "INI"
    
    /// instantiates the storyboard
    var instantiate: UIStoryboard {
        return UIStoryboard(name: rawValue, bundle: nil)
    }
    
    
}
