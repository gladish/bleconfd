//
//  BasePopoverViewController.swift
//  iOSClientApp
//
//  Created by Nikita Rodin on 12/18/18.
//  Copyright © 2018 Nikita Rodin. All rights reserved.
//

import UIKit
import RxCocoa
import RxSwift

class BasePopoverViewController: UIViewController {

    /// outlets
    @IBOutlet weak var titleLabel: UILabel!
    @IBOutlet weak var bgButton: UIButton!
    @IBOutlet weak var cancelButton: UIButton!
    @IBOutlet weak var sendButton: UIButton!

    /// shows popover
    ///
    /// - Returns: popover
    static func show(host: UIViewController? = nil, configuration: ((BasePopoverViewController)->())? = nil) -> Self? {
        guard let hostVC = host ?? UIViewController.current,
            let popover = hostVC.create(viewController: self) else { return nil }
        
        configuration?(popover)
        
        popover.modalPresentationStyle = .overCurrentContext
        hostVC.present(popover, animated: true, completion: nil)
        
        return popover
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        bgButton.alpha = 0
        UIView.animate(withDuration: 0.2, animations: { [weak self] in
            self?.bgButton.alpha = 1
        })
        
        Observable.merge(bgButton.rx.tap.asObservable(), cancelButton.rx.tap.asObservable())
            .subscribe(onNext: { [weak self] value in
                self?.dismiss()
            }).disposed(by: rx.bag)
    }

    /// dismiss popover
    func dismiss() {
        view.endEditing(true)
        UIView.animate(withDuration: 0.2, animations: { [ weak self] in
            self?.bgButton.alpha = 0
        }) { [weak self] _ in
            self?.dismiss(animated: true, completion: nil)
        }
    }
    
}
