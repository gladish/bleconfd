//
//  INIValuePopoverViewController.swift
//  iOSClientApp
//
//  Created by Nikita Rodin on 12/19/18.
//  Copyright © 2018 Nikita Rodin. All rights reserved.
//

import UIKit
import RxCocoa
import RxSwift

class INIValuePopoverViewController: BasePopoverViewController {

    /// outlets
    @IBOutlet weak var groupTextField: UITextField!
    @IBOutlet weak var keyTextField: UITextField!
    @IBOutlet weak var valueTextField: UITextField!
    
    /// the edited value group
    var group: String?
    /// the edited value
    var value: INIKeyValue?
    
    /// send handler
    var onSend: ((_ group: String, _ key: String, _ value: String)->())?
    
    override func viewDidLoad() {
        super.viewDidLoad()
    
        if let group = group,
            let edit = value {
            groupTextField.text = group
            keyTextField.text = edit.key
            valueTextField.text = edit.value
            titleLabel.text = "EDIT VALUE".localized
            sendButton.setTitle("EDIT".localized, for: .normal)
            groupTextField.isEnabled = false
            keyTextField.isEnabled = false
        }
        
        // disable send button until filled
        Observable.combineLatest(groupTextField.rx.text.orEmpty.startWith(group ?? ""), keyTextField.rx.text.orEmpty.startWith(value?.key ?? ""),
            valueTextField.rx.text.orEmpty.startWith(value?.value ?? ""))
            { !$0.trim().isEmpty && !$1.trim().isEmpty && !$2.trim().isEmpty }
            .bind(to: sendButton.rx.isEnabled)
            .disposed(by: rx.bag)
        
        // chain return buttons
        chain(textFields: [groupTextField, keyTextField, valueTextField], lastReturnKey: .send) { [weak self] in
            if self?.sendButton.isEnabled == true {
                self?.send()
            }
        }
        sendButton.rx.tap
            .subscribe(onNext: { [weak self] value in
                self?.send()
            }).disposed(by: rx.bag)
    }
    
    /// send credentials
    private func send() {
        dismiss()
        onSend?(groupTextField.textValue, keyTextField.textValue, valueTextField.textValue)
    }
    
    /// View will appear
    ///
    /// - Parameter animated: the animation flag
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        
        (group == nil ? groupTextField : valueTextField).becomeFirstResponder()
    }
    

}
