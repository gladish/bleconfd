//
//  WiFiPopoverViewController.swift
//  iOSClientApp
//
//  Created by Nikita Rodin on 12/18/18.
//  Copyright © 2018 Nikita Rodin. All rights reserved.
//

import UIKit
import RxCocoa
import RxSwift
import UIComponents

class WiFiPopoverViewController: BasePopoverViewController {

    /// outlets
    @IBOutlet weak var ssidTextField: UITextField!
    @IBOutlet weak var passwordTextField: UITextField!
    
    /// the SSID
    var ssid: String = ""
    
    /// send handler
    var onSend: ((_ ssid: String, _ password: String)->())?
    
    override func viewDidLoad() {
        super.viewDidLoad()

        ssidTextField.text = ssid
        // chain return buttons
        chain(textFields: [ssidTextField, passwordTextField], lastReturnKey: .send) { [weak self] in
            if self?.sendButton.isEnabled == true {
                self?.send()
            }
        }
        sendButton.rx.tap
            .subscribe(onNext: { [weak self] value in
                self?.send()
            }).disposed(by: rx.bag)
        // disable send button until filled
        Observable.combineLatest(ssidTextField.rx.text.orEmpty
                                    .startWith(ssid), passwordTextField.rx.text.orEmpty) { !$0.trim().isEmpty && !$1.isEmpty }
            .bind(to: sendButton.rx.isEnabled)
            .disposed(by: rx.bag)
    }
    
    /// send credentials
    private func send() {
        dismiss()
        onSend?(ssidTextField.textValue, passwordTextField.textValue)
    }

    /// View will appear
    ///
    /// - Parameter animated: the animation flag
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        
        (ssid.trim().isEmpty ? ssidTextField : passwordTextField).becomeFirstResponder()
    }
    
}
