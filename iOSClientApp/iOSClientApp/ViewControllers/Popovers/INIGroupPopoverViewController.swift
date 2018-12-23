//
//  INIGroupPopoverViewController.swift
//  iOSClientApp
//
//  Created by Nikita Rodin on 12/19/18.
//  Copyright © 2018 Nikita Rodin. All rights reserved.
//

import UIKit
import RxCocoa
import RxSwift

class INIGroupPopoverViewController: BasePopoverViewController {

    /// outlets
    @IBOutlet weak var groupTextField: UITextField!
    
    /// send handler
    var onSend: ((_ group: String)->())?
    
    /// the edited group
    var group: INIGroup?
    
    override func viewDidLoad() {
        super.viewDidLoad()

        if let edit = group {
            groupTextField.text = edit.name
            titleLabel.text = "EDIT GROUP".localized
            sendButton.setTitle("EDIT".localized, for: .normal)
        }
        
        // disable send button until filled
        groupTextField.rx.text.orEmpty
            .startWith(group?.name ?? "")
            .map { !$0.trim().isEmpty }
            .bind(to: sendButton.rx.isEnabled)
            .disposed(by: rx.bag)
        
        // chain return buttons
        chain(textFields: [groupTextField], lastReturnKey: .send) { [weak self] in
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
        onSend?(groupTextField.textValue)
    }
    
    /// View will appear
    ///
    /// - Parameter animated: the animation flag
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        
        groupTextField.becomeFirstResponder()
    }

}
