//
//  ContainerViewController.swift
//  iOSClientApp
//
//  Created by TCCODER on 9/1/17.
//  Copyright © 2018 Topcoder. All rights reserved.
//

import UIKit


class ContainerViewController: UIViewController {

    /// home menu
    var slideVC: SlideMenuViewController!
    var menuVC: MenuViewController!
    
    override func viewDidLoad() {
        super.viewDidLoad()

        // setup menu and default content
        menuVC = create(viewController: MenuViewController.self)
        let vc = create(viewController: HomeViewController.self, storyboard: .home)
        slideVC = SlideMenuViewController(leftSideController: menuVC, defaultContent: vc!)
        loadChildController(slideVC, inContentView: self.view)
        
        // handle menu change
        MenuItem.current.asObservable()
            .skip(1)
            .subscribe(onNext: { [weak self] value in
                guard let vc = self?.create(viewController: value.vc, storyboard: value.storyboard) else { return }
                self?.slideVC.setContentViewController(vc)
            }).disposed(by: rx.bag)
        
        
    }

}

/// shortcut for UIViewController
extension UIViewController {
    
    /// get parent container
    var containerVC: ContainerViewController? {
        return slideMenuController?.parent as? ContainerViewController
    }
    
}
