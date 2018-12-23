//
//  Logger.swift
//  iOSClientApp
//
//  Created by TCCODER on 1/24/16.
//  Copyright (c) 2018 TopCoder. All rights reserved.
//

import UIKit
import RxCocoa
import RxSwift

class Logger {
   
    
    /// Available log levels
    ///
    ///     - none:  None
    ///     - error: Errors only
    ///     - info:  No debug log
    ///     - debug: All logs
    ///
    enum LogLevel: Int {
        case none, error, info, debug
        
        /// Formatted title
        var title: String {
            switch self {
            case .none:
                return "[None]:\t"
            case .error:
                return "[Error]:\t"
            case .info:
                return "[Info]:\t"
            case .debug:
                return "[Debug]:\t"
            }
        }
    }
    
    // MARK: - interface
    
    /// copy of whole log
    static var allLog = ""
    static var allLogObs: Disposable?
    
    /// resets log
    class func reset() {
        allLog = ""
        Logger.allLogObs = Logger.feed(.debug)
            .subscribe(onNext: { value in
                Logger.allLog = value
            })
    }
    
    /// Logs message if current level is not less
    ///
    /// - Parameters:
    ///   - level: message level
    ///   - message: message text
    class func log(_ level: LogLevel, _ message: String) {
        guard logger.loggerLevel.rawValue >= level.rawValue else { return }
        
        let time = Date()
        Logger.logQueue.async {
            let logMessage = "\(Configuration.logIncludeTime ? "\(time.defaultFormat) " :  "")\(level.title) \(message)"
            Logger.allFeeds.accept(FeedItem(level: level, message: logMessage))
            if Configuration.logPrintConsole {
                print(logMessage)
            }
        }
    }
    
    /// provides readable feed
    ///
    /// - Parameter level: log level
    /// - Returns: readable feed
    class func feed(_ level: LogLevel) -> Observable<String> {
        var log = ""
        return Logger.allFeeds
            .filter { $0.level.rawValue <= level.rawValue }
            .map {
                log += "\($0.message)\n"
                return log
            }
            .observeOn(MainScheduler.instance)
            .share(replay: 1)
    }
    
    // MARK: - private
    
    /// feed item
    private struct FeedItem {
        /// fields
        let level: LogLevel
        let message: String
    }
    
    /// current log level
    private let loggerLevel = LogLevel(rawValue: Configuration.logLevel) ?? .debug
    
    /// log queue
    private static let logQueue = DispatchQueue(label: "com.bleconf.iOSClient.Logger.queue", qos: .background)
    
    /// all feeds
    private static let allFeeds = PublishRelay<FeedItem>()
    
    /// hidden initializer
    private init() {
        Logger.reset()
    }
    
    /// logger instance
    private static let logger = Logger()
    
}
