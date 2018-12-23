//
//  WiFiViewController.swift
//  iOSClientApp
//
//  Created by Nikita Rodin on 12/16/18.
//  Copyright © 2018 Nikita Rodin. All rights reserved.
//

import UIKit
import FGRoute
import RxCocoa
import RxSwift

class WiFiViewController: UIViewController {

    /// outlets
    @IBOutlet weak var wifiButton: UIButton!
    @IBOutlet weak var statusButton: UIButton!
    @IBOutlet weak var logsTextView: UITextView!
    
    /// current WiFi ssid
    private var ssid = FGRoute.getSSID() ?? ""
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        Logger.feed(.info)
            .subscribe(onNext: { [weak self] value in
                self?.logsTextView.text = value
            }).disposed(by: rx.bag)

        wifiButton.rx.tap
            .subscribe(onNext: { [weak self] value in
                guard let sself = self else { return }
                
                // update to current SSID if not empty
                let newSSID = FGRoute.getSSID() ?? ""
                if !newSSID.isEmpty {
                    sself.ssid = newSSID
                }
                
                guard let popover = WiFiPopoverViewController.show(host: sself, configuration: {
                    let popover = $0 as! WiFiPopoverViewController
                    popover.ssid = sself.ssid
                }) else { return }                
                
                popover.onSend = { [weak self] in
                    guard let sself = self else { return }
                    BLE.api.setWifi(ssid: $0, password: $1)
                        .showLoading(on: sself.view)
                        .subscribe(onNext: { value in
                            Logger.log(.info, value.message != nil ? "Set Wi-Fi response message: \(value.message!)"
                                : "Sent set Wi-Fi request \(value.code != 0 ? "with failure" : "successfully")")
                            }, onError: { error in
                        }).disposed(by: sself.rx.bag)
                }
                
            }).disposed(by: rx.bag)
        
        statusButton.rx.tap
            .subscribe(onNext: { [weak self] value in
                guard let sself = self else { return }
                
                BLE.api.getWifiStatus()
                    .showLoading(on: sself.view)
                    .subscribe(onNext: { value in
                        Logger.log(.info, "Get WiFi status: \(value.code != nil ? "disconnected, message: " : "connected ")\(value.message ?? "")")
                    }, onError: { error in
                }).disposed(by: sself.rx.bag)
            }).disposed(by: rx.bag)
    }
    
}
