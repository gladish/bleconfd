//
//  MenuViewController.swift
//  iOSClientApp
//
//  Created by TCCODER on 30/10/17.
//  Copyright © 2018 Topcoder. All rights reserved.
//

import UIKit
import RxCocoa
import RxSwift

/// menu items
enum MenuItem: String {
    case home = "Home",
        log = "Log",
        wifi = "Wi-Fi Settings",
        ini = "INI file"
    
    /// corresponding view controller
    var vc: UIViewController.Type {
        switch self {
        case .home:
            return HomeViewController.self
        case .log:
            return LogViewController.self
        case .wifi:
            return WiFiViewController.self
        case .ini:
            return INIEditorViewController.self
        }
    }
    
    /// corresponding storyboard
    var storyboard: Storyboards? {
        return Storyboards(rawValue: self.rawValue.components(separatedBy: CharacterSet.whitespaces)[0].replacingOccurrences(of: "-", with: ""))
    }
    
    /// current menu item
    static var current = Variable<MenuItem>(.home)
}

/// menu structure
class Menu {
    /// item
    let item: MenuItem
    /// item name
    let name: String
    /// is enabled
    var isEnabled = true
    
    /// default initializer
    init(item: MenuItem) {
        self.item = item
        self.name = item.rawValue.localized
    }
}

class MenuViewController: UIViewController {

    /// outlets
    @IBOutlet weak var tableView: UITableView!
    
    /// connection status
    var offline = false {
        didSet {
            tableView?.reloadData()
        }
    }

    /// viewmodel
    var vm = TableViewModel<Menu, MenuTableViewCell>()
    
    override func viewDidLoad() {
        super.viewDidLoad()
    
        // setup menu
        vm.entries.value = [.home, .log, .wifi, .ini].map { Menu(item: $0) }
        vm.configureCell = { _, menu, _, cell in
            cell.nameLabel.text = menu.name
            let selected = MenuItem.current.value == menu.item
            cell.nameLabel.textColor = menu.isEnabled ? (selected ? UIColor.black : UIColor.darkGray.withAlphaComponent(0.65)) : UIColor.lightGray.withAlphaComponent(0.65)
        }
        vm.onSelect = { [weak self] index, menu in
            guard menu.isEnabled else {
                self?.tableView.deselectSelected()
                return
            }
            MenuItem.current.value = menu.item
        }
        vm.bindData(to: tableView)
        MenuItem.current.asObservable()
            .subscribe(onNext: { [weak self] value in
                self?.tableView.reloadData()
            }).disposed(by: rx.bag)
        
        BLE.isConnected
            .subscribe(onNext: { [weak self] value in
                self?.disableMenus(disable: !value)
                }, onError: { [weak self] error in
                    self?.disableMenus()
            }).disposed(by: rx.bag)
    }
    
    /// disable menus if not connected
    private func disableMenus(disable: Bool = true) {
        if disable && (MenuItem.current.value == .ini
            || MenuItem.current.value == .wifi) {
            MenuItem.current.value = .home
        }
        vm.entries.value
            .filter { $0.item == .wifi || $0.item == .ini }
            .forEach { $0.isEnabled = !disable }
        tableView.reloadData()
    }
    
}

class MenuTableViewCell: UITableViewCell {
    
    /// outlets
    @IBOutlet weak var nameLabel: UILabel!

}
