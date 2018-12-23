//
//  AutoSizedTable.swift
//  iOSClientApp
//
//  Created by Nikita Rodin on 12/16/18.
//  Copyright © 2018 Nikita Rodin. All rights reserved.
//

import UIKit
import RxCocoa
import RxSwift

// MARK: - UIScrollView extension
extension Reactive where Base: UIScrollView {
    
    /// content size KVO
    var contentSize: Observable<CGSize> {
        return self.observe(CGSize.self, "contentSize")
            .flatten()
    }
    
}

/// utilize autolayout to size itself
class AutoSizedTable: UITableView {
    
    /// track content size changes
    override func awakeFromNib() {
        super.awakeFromNib()
        
        self.rx.contentSize
            .subscribe(onNext: { [weak self] value in
                self?.invalidateIntrinsicContentSize()
            }).disposed(by: rx.bag)
    }
    
    /// the view size
    override var intrinsicContentSize: CGSize {
        return contentSize
    }
    
}
