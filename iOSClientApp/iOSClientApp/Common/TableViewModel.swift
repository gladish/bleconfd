//
//  TableViewModel.swift
//
//  Created by TCCODER on 26/10/17.
//  Copyright © 2018 Topcoder. All rights reserved.
//

import UIKit
import RxCocoa
import RxSwift

class TableViewModel<ObjectClass, CellClass: UITableViewCell>: NSObject {

    /// initializer
    ///
    /// - Parameter value: initial values
    init(value: [ObjectClass] = []) {
        super.init()
        entries.value = value
    }
    
    /// track latest values for convenience
    var entries = Variable<[ObjectClass]>([])
    
    /// selection handler
    var onSelect: ((_ index: Int, _ value: ObjectClass) -> ())?
    
    /// cell configuration
    var configureCell: ((_ index: Int, _ value: ObjectClass, _ values: [ObjectClass], _ cell: CellClass) -> ())?
    
    /// binds data to table view
    ///
    /// - Parameter tableView: tableView to bind to
    /// - Parameter sequence: data sequence
    func bindData(to tableView: UITableView) {
        let sequence = entries.asObservable()
        sequence.bind(to: tableView.rx.items(cellIdentifier: CellClass.className, cellType: CellClass.self)) { [weak self] index, value, cell in
            guard let strongSelf = self else { return }
            let values = strongSelf.entries.value
            strongSelf.configureCell?(index, value, values, cell)
            }.disposed(by: rx.bag)
        // handle selection
        tableView.rx.itemSelected.subscribe(onNext: { [weak self] indexPath in
            guard let strongSelf = self else { return }
            let values = strongSelf.entries.value
            strongSelf.onSelect?(indexPath.row, values[indexPath.row])
        }).disposed(by: rx.bag)
    }
    
}
