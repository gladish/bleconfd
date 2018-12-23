//
//  LoadingView.swift
//
//  Created by TCCODER on 10/30/17.
//  Copyright © 2018 Topcoder. All rights reserved.
//

import UIKit

class LoadingView: UIView {
    
    /// activity indicator
    private var activityIndicator: UIActivityIndicatorView!
    
    /// description
    private var textLabel: UILabel!
    
    /// flag: true - the view is shown, false - else
    private var didShow = false
    
    /// the parent view
    private weak var parentView: UIView?
    
    /// Default initializer
    ///
    /// - Parameters:
    ///   - parentView: the parent view
    ///   - dim: true - dim background
    ///   - shift: shift added to the loading indicator center
    init(style: UIActivityIndicatorView.Style = .gray, parent: UIView?, dim: Bool = true, shift: CGSize = .zero) {
        super.init(frame: parent?.bounds ?? UIScreen.main.bounds)
        
        parentView = parentView ?? UIApplication.shared.delegate?.window!
        
        activityIndicator = UIActivityIndicatorView(style: style)
        activityIndicator.translatesAutoresizingMaskIntoConstraints = false
        addSubview(activityIndicator)
        activityIndicator.centerXAnchor.constraint(equalTo: centerXAnchor, constant: shift.width).isActive = true
        activityIndicator.centerYAnchor.constraint(equalTo: centerYAnchor, constant: shift.height).isActive = true
        
        backgroundColor = dim ? UIColor.black.withAlphaComponent(0.2) : .clear
        alpha = 0.0
    }
    
    
    /// Required initializer
    ///
    /// - Parameter aDecoder: decoder
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    /// Removes the loader from parent
    func remove() {
        if !didShow { return }
        UIView.animate(withDuration: 0.25, animations: {
            self.alpha = 0.0
        }, completion: { success in
            self.didShow = false
            self.activityIndicator.stopAnimating()
            self.removeFromSuperview()
        })
    }
    
    /// Show the loader
    ///
    /// - Returns: self
    func show() -> LoadingView {
        guard let view = parentView,
            !didShow else { return self }
        didShow = true
        translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(self)
        view.addConstraints(NSLayoutConstraint.constraints(withVisualFormat: "V:|-(\(0))-[view]-(\(0))-|", options: [], metrics: nil, views: ["view" : self]))
        view.addConstraints(NSLayoutConstraint.constraints(withVisualFormat: "H:|-(\(0))-[view]-(\(0))-|", options: [], metrics: nil, views: ["view" : self]))
        activityIndicator.startAnimating()
        UIView.animate(withDuration: 0.25) {
            self.alpha = 0.75
        }
        return self
    }
    
}
