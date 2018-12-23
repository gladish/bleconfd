//
//  IniEditorViewController.swift
//  iOSClientApp
//
//  Created by Nikita Rodin on 12/16/18.
//  Copyright © 2018 Nikita Rodin. All rights reserved.
//

import UIKit
import RxCocoa
import RxSwift
import RxGesture
import UIComponents

class INIEditorViewController: UIViewController {

    /// outlets
    @IBOutlet weak var tableView: UITableView!
    @IBOutlet weak var getAllButton: UIButton!
    @IBOutlet weak var addGroupButton: UIButton!
    @IBOutlet weak var addKeyButton: UIButton!
    
    /// the groups
    private var groups = Variable<[INIGroup]>([])
    
    /// viewmodel
    private var vm = TableViewModel<INIItem, INIItemCell>()
    
    /// fetch group name helper
    private var groupForItem: [String] = []
    
    /// swipe thresholds
    private struct Constants {
        fileprivate static let threshold: CGFloat = 0.35
        fileprivate static let velocityMultiplier: CGFloat = 0.1
        fileprivate static let velocityThreshold: CGFloat = 0.65
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()

        if IS_IPHONE5 {
            addKeyButton.setTitle("ADD VALUE", for: .normal)
        }
        
        setupActions()
        setupTable()
    }
    
    /// setup actions
    private func setupActions() {
        getAllButton.rx.tap
            .subscribe(onNext: { [weak self] value in
                guard let sself = self else { return }
                BLE.api.getINIGroups()
                    .showLoading(on: sself.view)
                    .subscribe(onNext: { [weak self] value in
                        self?.groups.value = value
                        }, onError: { error in
                    }).disposed(by: sself.rx.bag)
            }).disposed(by: rx.bag)
        
        addKeyButton.rx.tap
            .subscribe(onNext: { [weak self] value in
                guard let sself = self,
                    let popover = INIValuePopoverViewController.show(host: sself) else { return }
                
                popover.onSend = { [weak self] in
                    guard let sself = self else { return }
                    BLE.api.addOrEdit(key: $1, value: $2, group: $0)
                        .showLoading(on: sself.view)
                        .subscribe(onNext: { value in
                            self?.groups.value = value
                        }, onError: { error in
                        }).disposed(by: sself.rx.bag)
                }
            }).disposed(by: rx.bag)
        
        addGroupButton.rx.tap
            .subscribe(onNext: { [weak self] value in
                guard let sself = self,
                    let popover = INIGroupPopoverViewController.show(host: sself) else { return }
                
                popover.onSend = { [weak self] in
                    guard let sself = self else { return }
                    BLE.api.add(group: $0)
                        .showLoading(on: sself.view)
                        .subscribe(onNext: { value in
                            self?.groups.value = value
                        }, onError: { error in
                        }).disposed(by: sself.rx.bag)
                }
            }).disposed(by: rx.bag)
    }
    
    /// setup table
    private func setupTable() {
        groups.asObservable()
            .map { [weak self] in
                self?.groupForItem.removeAll()
                return $0.flatMap { group -> [INIItem] in
                    var items: [INIItem] = [group]
                    items.append(contentsOf: group.keyValues)
                    // mark corresponding group for each item
                    for _ in items {
                        self?.groupForItem.append(group.name)
                    }
                    return items
                }
            }
            .bind(to: vm.entries)
            .disposed(by: rx.bag)
        
        vm.configureCell = { index, item, _, cell in
            cell.headerStyle = item.isHeader
            cell.titleLabel.text = item.title
            cell.restoreLayout()
            
            // panning
            let panGesture = cell.containerView.rx
                .panGesture()
                .share(replay: 1)
            panGesture
                .when(.changed)
                .asTranslation()
                .subscribe(onNext: { [unowned cell] value in
                    let isEdit = value.translation.x < 0
                    let percent = (abs(value.translation.x) / cell.contentView.frame.width)
                    
                    UIView.animate(withDuration: 0.1) {
                        cell.leftOffset.constant = value.translation.x
                        cell.rightOffset.constant = -value.translation.x
                        cell.deleteIcon.isHidden = isEdit
                        cell.editIcon.isHidden = !isEdit
                        cell.contentView.backgroundColor = (isEdit ? UIColor.blue : UIColor.red).withAlphaComponent(percent)
                        cell.contentView.layoutIfNeeded()
                    }
                })
                .disposed(by: cell.rx.reuseBag)
            panGesture
                .when(.ended)
                .asTranslation()
                .subscribe(onNext: { [unowned cell, weak self] value in
                    let isEdit = value.translation.x < 0
                    let percent = (abs(value.translation.x) / cell.contentView.frame.width)
                    let velocityPercent = (abs(value.velocity.x) / cell.contentView.frame.width) * Constants.velocityMultiplier
                    if percent > Constants.threshold
                        && percent + velocityPercent > Constants.velocityThreshold {
                        let translation = isEdit ? -cell.contentView.frame.width : cell.contentView.frame.width
                        // snap to action
                        UIView.animate(withDuration: max(0.1, min(0.3, TimeInterval((1.0 - percent) / velocityPercent * Constants.velocityMultiplier))), animations: {
                            cell.leftOffset.constant = translation
                            cell.rightOffset.constant = -translation
                            cell.deleteIcon.isHidden = isEdit
                            cell.editIcon.isHidden = !isEdit
                            cell.contentView.backgroundColor = (isEdit ? UIColor.blue : UIColor.red).withAlphaComponent(percent)
                            cell.contentView.layoutIfNeeded()
                        }) { _ in
                            self?.cellAction(index: index, item: item, isEdit: isEdit)
                        }
                    }
                    else {
                        // snap back
                        UIView.animate(withDuration: 0.2) {
                            cell.restoreLayout()
                            cell.contentView.layoutIfNeeded()
                        }
                    }
                    
                })
                .disposed(by: cell.rx.reuseBag)
            
        }
        vm.bindData(to: tableView)
    }
    
    /// perform cell action
    ///
    /// - Parameters:
    ///   - item: cell item
    ///   - isEdit: is edit action
    private func cellAction(index: Int, item: INIItem, isEdit: Bool) {
        if let group = item as? INIGroup  { // group actions
            // edit
            if isEdit {
                guard let popover = INIGroupPopoverViewController.show(host: self, configuration: {
                    let popover = $0 as! INIGroupPopoverViewController
                    popover.group = group
                }) else { return }
                
                popover.onSend = { [weak self] in
                    guard let sself = self else { return }
                    BLE.api.edit(group: group.name, newName: $0)
                        .showLoading(on: sself.view)
                        .subscribe(onNext: { value in
                            self?.groups.value = value
                        }, onError: { error in
                        }).disposed(by: sself.rx.bag)
                }
            }
            else { // delete
                confirm(action: "", message: "Are you sure you want delete group \(group.name) ?", confirmTitle: "Delete".localized,
                        confirmHandler: { [weak self] in
                            guard let sself = self else { return }
                            BLE.api.delete(group: group.name)
                                .showLoading(on: sself.view)
                                .subscribe(onNext: { value in
                                    self?.groups.value = value
                                }, onError: { error in
                                }).disposed(by: sself.rx.bag)
                }, cancelHandler: nil)
            }
        }
        else if let keyValue = item as? INIKeyValue { // KeyValue actions
            let group = groupForItem[index]
            // edit
            if isEdit {
                guard let popover = INIValuePopoverViewController.show(host: self, configuration: {
                    let popover = $0 as! INIValuePopoverViewController
                    popover.group = group
                    popover.value = keyValue
                }) else { return }
                
                popover.onSend = { [weak self] in
                    guard let sself = self else { return }
                    BLE.api.addOrEdit(key: $1, value: $2, group: $0)
                        .showLoading(on: sself.view)
                        .subscribe(onNext: { value in
                            self?.groups.value = value
                        }, onError: { error in
                        }).disposed(by: sself.rx.bag)
                }
            }
            else { // delete
                confirm(action: "", message: "Are you sure you want delete this value (key=\(keyValue.key)) ?", confirmTitle: "Delete".localized,
                        confirmHandler: { [weak self] in
                            guard let sself = self else { return }
                            BLE.api.delete(key: keyValue.key, group: group)
                                .showLoading(on: sself.view)
                                .subscribe(onNext: { value in
                                    self?.groups.value = value
                                }, onError: { error in
                                }).disposed(by: sself.rx.bag)
                    }, cancelHandler: nil)
            }
        }
        
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) { [weak self] in
            self?.tableView.reloadData()
        }
    }
    
}

class INIItemCell: UITableViewCell {
    
    /// outlets
    @IBOutlet weak var containerView: UIView!
    @IBOutlet weak var titleLabel: UILabel!
    @IBOutlet weak var textLeftOffset: NSLayoutConstraint!
    @IBOutlet weak var leftOffset: NSLayoutConstraint!
    @IBOutlet weak var rightOffset: NSLayoutConstraint!
    @IBOutlet weak var editIcon: UIImageView!
    @IBOutlet weak var deleteIcon: UIImageView!
    
    /// restores original state
    func restoreLayout() {
        leftOffset.constant = 0
        rightOffset.constant = 0
        contentView.backgroundColor = UIColor.white
        editIcon.isHidden = false
        deleteIcon.isHidden = false
    }
    
    /// apply header header
    var headerStyle: Bool {
        get { return false }
        set {
            if newValue {
                textLeftOffset.constant = 15;
                titleLabel.textColor = UIColor.from(hexString: "008577")
                titleLabel.font = UIFont.systemFont(ofSize: titleLabel.font.pointSize, weight: .semibold)
            }
            else {
                textLeftOffset.constant = 25;
                titleLabel.textColor = .black
                titleLabel.font = UIFont.systemFont(ofSize: titleLabel.font.pointSize)
            }
        }
    }
    
}
