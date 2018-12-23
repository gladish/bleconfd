//
//  ViewController.swift
//  iOSClientApp
//
//  Created by Nikita Rodin on 12/16/18.
//  Copyright © 2018 Nikita Rodin. All rights reserved.
//

import UIKit
import RxCocoa
import RxSwift
import RxBluetoothKit
import CoreBluetooth

class HomeViewController: UIViewController {

    /// outlets
    @IBOutlet weak var statusLabel: UILabel!
    @IBOutlet weak var bgButton: UIButton!
    @IBOutlet weak var selectButton: UIButton!
    @IBOutlet weak var connectButton: UIButton!
    @IBOutlet weak var disconnectButton: UIButton!
    @IBOutlet weak var tableView: UITableView!
    @IBOutlet weak var shadowView: UIView!
    @IBOutlet weak var logsTextView: UITextView!
        
    /// viewmodel
    private var vm = TableViewModel<Peripheral, MenuTableViewCell>()
    
    /// concurrent scheduler
    private let scheduler = ConcurrentDispatchQueueScheduler.init(qos: .default)
    
    /// last scan date
    private var lastScan: Date?
    
    /// scan cancel disposable
    private var scan: Disposable?
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        Logger.feed(.info)
            .subscribe(onNext: { [weak self] value in
                self?.logsTextView.text = value
            }).disposed(by: rx.bag)
        
        setupStateObserving()
        setupTable()
        setupActions()
    }
    
    /// setup state observing
    private func setupStateObserving() {
        BLE.manager.observeState()
            .subscribe(onNext: { [weak self] value in
                let status = String(reflecting: value)
                Logger.log(.debug, "CBCentralManager state changed: \(status)")
                let name = status.components(separatedBy: ",").last!
                self?.statusLabel.text = value == .poweredOn ? "" : "Bluetooth state is \(name)"
            }).disposed(by: rx.bag)
        
        BLE.peripheral.asObservable()
            .subscribe(onNext: { [weak self] value in
                self?.selectButton.setTitle(value?.name ?? "Select".localized, for: .normal)
                self?.connectButton.isEnabled = value != nil && !value!.isConnected
                self?.disconnectButton.isEnabled = value?.isConnected == true
            }).disposed(by: rx.bag)
        BLE.isConnected
            .subscribe(onNext: { [weak self] value in
                let isConnected = BLE.peripheral.value?.isConnected == true
                self?.connectButton.isEnabled = !isConnected && BLE.peripheral.value != nil
                self?.disconnectButton.isEnabled = isConnected
                }, onError: { [weak self] error in
                    self?.connectButton.isEnabled = BLE.peripheral.value != nil
                    self?.disconnectButton.isEnabled = false
            }).disposed(by: rx.bag)
    }

    /// setup actions
    private func setupActions() {
        bgButton.rx.tap
            .subscribe(onNext: { [weak self] value in
                self?.tableView.isHidden = true
                self?.scan?.dispose()
            }).disposed(by: rx.bag)
        
        connectButton.rx.tap
            .subscribe(onNext: { [weak self] in
                guard let p = BLE.peripheral.value else { return }
                
                self?.connectButton.isEnabled = false
                p.connectGatt()
            }).disposed(by: rx.bag)
        
        disconnectButton.rx.tap
            .subscribe(onNext: { [weak self] in
                guard let p = BLE.peripheral.value else { return }
                
                self?.disconnectButton.isEnabled = false
                p.disconnect()
            }).disposed(by: rx.bag)
        
        selectButton.rx.tap
            .subscribe(onNext: { [weak self] value in
                guard let sself = self,
                    BLE.manager.state == .poweredOn else {
                        self?.showErrorAlert(message: "Bluetooth is not powered on".localized)
                        return
                }
                
                sself.tableView.isHidden = false
                sself.startScan()
                
            }).disposed(by: rx.bag)
    }
    
    /// setup table
    private func setupTable() {
        tableView.isHidden = true
        
        // grab first (paired or found) if configured
        if Configuration.autoPair && BLE.peripheral.value == nil {
            vm.entries.asObservable()
                .filter { !$0.isEmpty }
                .take(1)
                .flatMap { Observable.from($0) }
                .take(1)
                .subscribe(onNext: {
                    BLE.peripheral.value = $0
                }).disposed(by: rx.bag)
        }
        
        BLE.manager.observeState()
            .startWith(BLE.manager.state)
            .filter { $0 == .poweredOn }
            .take(1)
            .subscribe(onNext: { [weak self] value in
                guard let sself = self else { return }
                Logger.log(.info, "Retrieving paired devices")
                // get paired
                let paired = BLE.manager.retrieveConnectedPeripherals(withServices: [Configuration.rpcServiceUUID])
                Logger.log(.info, "Found paired devices: \(paired.map { $0.name ?? $0.peripheral.identifier.uuidString })")
                sself.vm.entries.value = paired
                
                // auto-find if configured
                if sself.vm.entries.value.isEmpty && Configuration.autoPair && BLE.peripheral.value == nil {
                    sself.startScan(takeOne: true)
                }
            }).disposed(by: rx.bag)
        
        
        vm.configureCell = { _, item, _, cell in
            cell.nameLabel.text = item.name
        }
        vm.onSelect = { [weak self] _, item in
            BLE.peripheral.value = item
            self?.tableView.isHidden = true
            self?.scan?.dispose()
        }
        vm.bindData(to: tableView)
        // show shadow
        tableView.rx.observe(Bool.self, "hidden")
            .flatten()
            .bind(to: shadowView.rx.isHidden)
            .disposed(by: rx.bag)
    }
    
    // perform cancellable scan
    private func startScan(takeOne: Bool = false) {
        let scanning = BLE.manager.scanForPeripherals(withServices: Configuration.deviceAdvertisedUUIDs)
        scan = (takeOne ? scanning.take(1) : scanning)
            .do(onSubscribed: { Logger.log(.info, "Started scanning") })
            .subscribe(onNext: { [weak self] value in
                if self?.vm.entries.value.contains(value.peripheral) == false {
                    Logger.log(.debug, "Found device \((value.advertisementData.localName ?? value.peripheral.identifier.uuidString)) with services \(value.advertisementData.serviceUUIDs?.map { $0.uuidString } ?? [])")
                    
                    self?.vm.entries.value.append(value.peripheral)
                }
                }, onError: { error in
            }, onDisposed: {
                Logger.log(.info, "Finished scanning")
            })
    }

}
