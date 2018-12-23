//
//  UIExtensions.swift
//
//  Created by TCCODER on 30/10/17.
//  Copyright © 2018  topcoder. All rights reserved.
//

import UIKit

/// convenience shortcut
func showAlert(title: String, message: String, handler: ((UIAlertAction) -> Void)? = nil) {
    UIViewController.current?.showAlert(title: title, message: message, handler: handler)
}

// MARK: - shortcut methods for UIViewController
extension UIViewController {
    
    /// instantiates initial viewController from storyboard
    ///
    /// - Returns: viewController if exists
    func createInitialViewController(storyboard: Storyboards) -> UIViewController? {
        return storyboard.instantiate.instantiateInitialViewController()
    }
    
    /// instantiates viewController from storyboard
    ///
    /// - Parameter viewController: viewController Type
    /// - Parameter storyboard: storyboard or nil to use current one
    /// - Returns: viewController if exists
    func create<T: UIViewController>(viewController: T.Type, storyboard: Storyboards? = nil) -> T? {
        if let storyboard = storyboard {
            return storyboard.instantiate.instantiateViewController(withIdentifier: viewController.className) as? T
        }
        return self.storyboard?.instantiateViewController(withIdentifier: viewController.className) as? T
    }
    
    /// Load child controller inside contentView
    ///
    /// - Parameters:
    ///   - vc: child controller
    ///   - contentView: container view
    ///   - insets: container view insets
    func loadChildController(_ vc: UIViewController, inContentView contentView: UIView, insets: UIEdgeInsets = .zero) {
        self.addChild(vc)
        
        vc.view.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(vc.view)
        contentView.addConstraints(NSLayoutConstraint.constraints(withVisualFormat: "V:|-(\(insets.top))-[view]-(\(insets.bottom))-|", options: [], metrics: nil, views: ["view" : vc.view]))
        contentView.addConstraints(NSLayoutConstraint.constraints(withVisualFormat: "H:|-(\(insets.left))-[view]-(\(insets.right))-|", options: [], metrics: nil, views: ["view" : vc.view]))
        
        self.view.layoutIfNeeded()
        
        vc.didMove(toParent: self)
    }

    /// Unload child controller from its parent
    ///
    /// - Parameter vc: child controller
    func unloadChildController(_ vc: UIViewController?) {
        if let vc = vc {
            vc.willMove(toParent: nil)
            vc.removeFromParent()
            vc.view.removeFromSuperview()
        }
    }
    
    /// unloads first child controller if present
    func unloadCurrentChildController() {
        unloadChildController(children.first)
    }
    
    /// Show alert
    ///
    /// - Parameters:
    ///   - title: the title of the alert
    ///   - message: the message of the alert
    ///   - handler: the alert dismiss handler
    func showAlert(title: String, message: String, handler: ((UIAlertAction) -> Void)? = nil) {
        let alert = UIAlertController(title: title, message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "OK".localized, style: .default, handler: handler))
        
        present(alert, animated: true, completion: nil)
    }
    
    /// Show error alert
    ///
    /// - Parameters:
    ///   - message: the message of the error alert
    ///   - handler: the alert dismiss handler
    func showErrorAlert(message: String, handler: ((UIAlertAction) -> Void)? = nil) {
        showAlert(title: "Error".localized, message: message, handler: handler)
    }
    
    /// shows alert to confirm an user action
    ///
    /// - Parameters:
    ///   - action: action title
    ///   - message: confirmation message
    ///   - confirmTitle: confirmation button title, default "OK"
    ///   - cancelTitle: cancel button title, default "Cancel"
    ///   - confirmHandler: confirmation handler
    ///   - cancelHandler: optional cancel handler
    func confirm(action: String, message: String, confirmTitle: String = "OK".localized, cancelTitle: String = "Cancel".localized, confirmHandler: @escaping ()->(), cancelHandler: (()->())?) {
        let alert = UIAlertController(title: action, message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: confirmTitle, style: .destructive, handler: { _ in
            confirmHandler()
        }))
        alert.addAction(UIAlertAction(title: cancelTitle, style: .cancel, handler: { _ in
            cancelHandler?()
        }))
        present(alert, animated: true, completion: nil)
        
    }
    
    /// view controller to present on
    static var current: UIViewController? {
        var vc = UIApplication.shared.delegate?.window??.rootViewController
        while vc?.presentedViewController != nil {
            vc = vc?.presentedViewController
        }
        return vc
    }
    
    /// wraps view controller in a navigation controller
    var wrappedInNavigationController: UINavigationController {
        let navigation = UINavigationController(rootViewController: self)
        return navigation
    }
    
}

// MARK: - table view extensions
extension UITableView {
    
    /// deselects selected row
    ///
    /// - Parameter animated: animated or not
    func deselectSelected(animated: Bool = true) {
        if let indexPath = indexPathForSelectedRow {
            deselectRow(at: indexPath, animated: animated)
        }
    }
    
    /**
     Get cell of given class for indexPath
     
     - parameter indexPath: the indexPath
     - parameter cellClass: a cell class
     
     - returns: a reusable cell
     */
    func getCell<T: UITableViewCell>(_ indexPath: IndexPath, ofClass cellClass: T.Type) -> T {
        return self.dequeueReusableCell(withIdentifier: cellClass.className, for: indexPath) as! T
    }
    
}

// MARK: - text field extensions
extension UITextField {
    
    /// unwrapped text value
    var textValue: String {
        return text ?? ""
    }
    
}


/// screen size
let SCREEN_SIZE = UIScreen.main.bounds.size

/// is iPad
let IS_IPAD = UIDevice.current.userInterfaceIdiom == .pad

/// iphone <=5 flag
let IS_IPHONE5 = UIScreen.main.bounds.height <= 568
