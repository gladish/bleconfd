//
//  BLEService.swift
//  iOSClientApp
//
//  Created by Nikita Rodin on 12/17/18.
//  Copyright © 2018 Nikita Rodin. All rights reserved.
//

import RxCocoa
import RxSwift
import RxBluetoothKit
import RxGesture
import CoreBluetooth
import SwiftyJSON

typealias Byte = UInt8

/// namespace
struct BLE {
    
    /// service
    static fileprivate let service = BLEService()
    /// central manager
    static let manager = CentralManager(queue: .main)
    /// API
    static let api = BLEAPI()
    
    /// packet delimiter
    static fileprivate let packetDelimiter: Byte = 30
    
    /// selected peripheral
    static let peripheral = Variable<Peripheral?>(nil)
    
    /// is selected peripheral connected
    static var isConnected: Observable<Bool> {
        return BLE.peripheral.asObservable()
                .flatten()
                .flatMapLatest { $0.observeConnection() }
                .startWith(false)
                .share(replay: 1)
    }
    
}

/// handles low-level communication
private class BLEService: HasDisposeBag {
    
    /// inbox
    fileprivate var inboxMessages: Observable<InboxPacket> {
        return inboxMessagesSubject.asObservable()
            .share(replay: 1)
    }
    fileprivate var inbox = PublishSubject<Data>()
    /// inbox internal
    private var inboxMessagesSubject = PublishSubject<InboxPacket>()
    private var inboxData = [Byte]()
    private let inboxScheduler = SerialDispatchQueueScheduler(qos: .default)
    private let pollingScheduler = SerialDispatchQueueScheduler(qos: .default)
    
    /// connected peripheral
    fileprivate var peripheral: Peripheral?
    
    /// poll service
    private var pollService: Disposable?
    
    /// connected characteristics
    fileprivate var characteristics = [CBUUID: Characteristic]()
    
    /// hidden initializer
    fileprivate override init() {
        super.init()
        inbox.asObservable()
            .subscribeOn(inboxScheduler)
            .observeOn(inboxScheduler)
            .subscribe(onNext: { [weak self] value in
                self?.inboxData += value
                if value.contains(BLE.packetDelimiter) {
                    self?.parseInbox()
                }
            })
            .disposed(by: bag)
    }
 
    /// attempt to read inbox characteristic
    fileprivate func readInbox() {
        guard let inboxChar = characteristics[Configuration.rpcInboxUUID] else { return }
        
        inboxChar.readValue()
            .subscribe(onSuccess: {
                guard let bytes = $0.value, !bytes.isEmpty else { return }
                
                Logger.log(.debug, "Inbox received bytes, length = \(bytes.count)")
                BLE.service.inbox.onNext(bytes)
                }, onError: { error in
                    Logger.log(.error, "Failed to read inbox \(error.localizedDescription)")
            }).disposed(by: bag)
        
        // read will occur on
        peripheral?.peripheral.readValue(for: inboxChar.characteristic)
    }
    
    /// writes data to inbox
    ///
    /// - Parameter data: the data
    fileprivate func writeInbox(data: Data) -> Observable<InboxPacket> {
        guard let inboxChar = characteristics[Configuration.rpcInboxUUID] else { return Observable.error("Inbox not available") }
        
        let response = inboxMessages
        return inboxChar.writeValue(data + [BLE.packetDelimiter], type: .withResponse)
            .asObservable()
            .flatMap { _ in response }
    }
    
    /// parses inbox data
    private func parseInbox() {
        guard let index = inboxData.firstIndex(of: BLE.packetDelimiter) else { return }
        let packet = inboxData[..<index]
        inboxData.removeFirst(index+1)
        let packetData = Data(packet)
        if let message = try? JSON(data: packetData) {
            inboxMessagesSubject.onNext(InboxPacket(json: message, originalString: String(data: packetData, encoding: .utf8) ?? ""))
            Logger.log(.debug, "Parsed inbox message:\n\(message)")
        }
    }
    
    /// setup poll service
    fileprivate func startPolling() {
        guard let pollChar = characteristics[Configuration.ePollUUID] else { return }
        
        Logger.log(.info, "Completed service discovery. Starting polling...")
        
        pollService = Observable<Int>.timer(Configuration.pollingDelay, period: Configuration.pollingDelay, scheduler: pollingScheduler)
            .flatMap { _ in pollChar.readValue() }
            .subscribe(onNext: {
                guard let bytes = $0.value, !bytes.isEmpty else { return }

                guard bytes.count >= 4 else { return }

                var length = Int(bytes[0])
                length <<= 1
                length += Int(bytes[1])
                length <<= 1
                length += Int(bytes[2])
                length <<= 1
                length += Int(bytes[3])

                if length > 0 {
                    Logger.log(.debug, "Got content notification, content length = \(length)")
                    BLE.service.readInbox()
                }
                }, onError: { error in
                    Logger.log(.error, "Failed to read poll service \(error.localizedDescription)")
            })
    }
    
    /// disconnect current peripheral
    fileprivate func disconnect() {
        Logger.log(.info, "Disconnecting...")
        pollService?.dispose()
        characteristics.removeAll()
        if let p = peripheral?.peripheral {
            BLE.manager.manager.cancelPeripheralConnection(p)
        }
        peripheral = nil
    }
    
}

// MARK: - shortcuts for peripheral connection
extension Peripheral {
    
    /// connect gatt service
    func connectGatt() {
        establishConnection()
            .do(onNext: { p in
                BLE.service.peripheral = p
                BLE.service.characteristics.removeAll()
                Logger.log(.info, "Started service discovery for \(p.peripheral.name ?? "peripheral")")
            })
            .flatMap { $0.discoverServices([Configuration.rpcServiceUUID]) }
            .flatMap { Observable.from($0) }
            .do(onNext: { Logger.log(.debug, "Discovered service \($0.uuid)") })
            .flatMap { $0.discoverCharacteristics([Configuration.rpcInboxUUID, Configuration.ePollUUID]) }
            .flatMap { Observable.from($0) }
            .do(onNext: { BLE.service.characteristics[$0.uuid] = $0 })
            .subscribe(onNext: {
                BLE.service.characteristics[$0.uuid] = $0
                
                Logger.log(.debug, "Discovered characteristic \($0.uuid)")
                
                if let _ = BLE.service.characteristics[Configuration.rpcInboxUUID],
                    let _ = BLE.service.characteristics[Configuration.ePollUUID] {
                    BLE.service.startPolling()
                }
                
            }, onError: { error in
                Logger.log(.error, String(reflecting: error))
            })
            .disposed(by: BLE.service.bag)
    }
    
    /// disconnect
    func disconnect() {
        guard BLE.service.peripheral === self else { return }
        BLE.service.disconnect()
    }
    
}

/// provides high-level API
class BLEAPI {
    
    /// hidden initializer
    fileprivate init() {}    
    
    /// sends request
    ///
    /// - Parameters:
    ///   - request: request name
    ///   - parameters: parameters
    ///   - resultKey: filters response
    /// - Returns: result observable
    func send(request: String, parameters: [String: Any], resultKey key: String? = nil) -> Observable<InboxPacket> {
        var params = parameters
        params["jsonrpc"] = "2.0"
        params["method"] = request
        let requestId = Int.random(in: 1...Int(10e8))
        params["id"] = requestId
        
        Logger.log(.debug, "Sending request \(params)")
        
        return Observable.just(JSON(params))
            .flatMap { BLE.service.writeInbox(data: try $0.rawData()) }
            .filter { $0.json["id"].int == requestId || (key != nil && $0.json[key!, "id"].int == requestId) }
    }
    
}
