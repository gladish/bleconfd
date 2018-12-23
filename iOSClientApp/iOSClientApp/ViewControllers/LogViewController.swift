//
//  LogViewController.swift
//  iOSClientApp
//
//  Created by Nikita Rodin on 12/16/18.
//  Copyright © 2018 Nikita Rodin. All rights reserved.
//

import UIKit
import RxCocoa
import RxSwift

class LogViewController: UIViewController {

    /// outlets
    @IBOutlet weak var logsTextView: UITextView!
    @IBOutlet weak var resetButton: UIBarButtonItem!
    
    override func viewDidLoad() {
        super.viewDidLoad()

        logsTextView.text = Logger.allLog
        
        resetButton.rx.tap
            .subscribe(onNext: { [weak self] value in
                Logger.reset()
                self?.logsTextView.text = ""
            }).disposed(by: rx.bag)
        
        Logger.feed(.debug)
            .subscribe(onNext: { [weak self] value in
                self?.logsTextView.text = Logger.allLog
            }).disposed(by: rx.bag)
    }
    
}
