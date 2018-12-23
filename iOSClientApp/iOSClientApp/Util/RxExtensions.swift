//
//  RxExtensions.swift
//
//  Created by TCCODER on 30/10/17.
//  Copyright © 2018 Topcoder. All rights reserved.
//

import UIKit
import RxCocoa
import RxSwift
import NSObject_Rx

// MARK: - reactive extension for NSObject
extension Reactive where Base: NSObject {
    
    /// shorthand
    var bag: DisposeBag {
        return disposeBag
    }

}

/// if you don't want to extend NSObject
class HasDisposeBag {
    
    /// dispose bag
    var bag = DisposeBag()
    
}

extension Reactive where Base: HasDisposeBag {
    
    /// shorthand
    var bag: DisposeBag {
        return base.bag
    }
    
}

//// MARK: - Error
extension String : Error {}


// MARK: - flatten
extension Observable where Element: Optionable {
    
    /// flattens sequence
    ///
    /// - Returns: flattened observable
    func flatten() -> Observable<Element.Wrapped>  {
        return self
            .filter { e in
                switch e.value {
                case .some(_):
                    return true
                default:
                    return false
                }
            }
            .map { $0.value! }
        
    }
    
}
/// Optionable protocol exposes the subset of functionality required for flatten definition
protocol Optionable {
    associatedtype Wrapped
    var value: Wrapped? { get }
}

/// extension for Optional provides the implementations for Optional enum
extension Optional : Optionable {
    var value: Wrapped? { get { return self } }
}

// MARK: - shorcuts
extension Observable {
    
    /// shows loading from NOW until error/completion/<optionally> next item; assumes observing on main scheduler
    ///
    /// - Parameter view: view to insert loader into
    /// - Parameter showAlertOnError: set to false to disable showing error alert
    /// - Returns: same observable
    func showLoading(on view: UIView, showAlertOnError: Bool = true, stopOnNext: Bool = false) -> Observable<Element> {
        let loader = LoadingView(parent: view).show()
        return self.do(onNext: { _ in
            if stopOnNext {
                loader.remove()
            }
        }, onError: { (error) in
            if showAlertOnError {
                let message = error as? String ?? error.localizedDescription
                showAlert(title: "Error".localized, message: message)
            }
            loader.remove()
        }, onCompleted: {
            loader.remove()
        })
    }
    
}

// MARK: - UIViewController extension
extension UIViewController {
    
    /// chains textfields to proceed between them one by one
    ///
    /// - Parameters:
    ///   - textFields: array of textFields
    ///   - lastReturnKey: last return key type
    ///   - lastHandler: handler for last return
    func chain(textFields: [UITextField], lastReturnKey: UIReturnKeyType = .default, lastHandler: (()->())? = nil) {
        let n = textFields.count
        for (i, tf) in textFields.enumerated() {
            if i < n-1 {
                tf.returnKeyType = .next
                tf.rx.controlEvent(.editingDidEndOnExit)
                    .subscribe(onNext: { value in
                        textFields[i+1].becomeFirstResponder()
                    }).disposed(by: rx.bag)
            }
            else {
                tf.returnKeyType = lastReturnKey
                tf.rx.controlEvent(.editingDidEndOnExit)
                    .subscribe(onNext: { value in
                        lastHandler?()
                    }).disposed(by: rx.bag)
            }
            
        }
    }
    
}
