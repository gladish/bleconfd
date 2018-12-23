//
//  UIExtensions.swift
//  Thoroughbred
//
//  Created by TCCODER on 30/10/17.
//  Modified by TCCODER on 2/23/18.
//  Copyright © 2018  topcoder. All rights reserved.
//

import UIKit

// MARK: - Interface Builder
extension UIView {
    
    /// corner radius (all corners)
    @IBInspectable var cornerRadius: CGFloat {
        get {
            return layer.cornerRadius
        }
        set {
            layer.cornerRadius = newValue            
        }
    }
    
    /// rounds just the specified corners
    ///
    /// - Parameters:
    ///   - corners: corners to round
    ///   - radius: the radius
    func roundCorners(corners: UIRectCorner, radius: CGFloat) {
        let maskPath = UIBezierPath(roundedRect: bounds,
                                    byRoundingCorners: corners,
                                    cornerRadii: CGSize(width: radius, height: radius))
        
        let shape = CAShapeLayer()
        shape.path = maskPath.cgPath
        layer.mask = shape
    }
    
    /// border width
    @IBInspectable var borderWidth: CGFloat {
        get {
            return layer.borderWidth
        }
        set {
            layer.borderWidth = newValue
        }
    }
    
    
    /// border color
    @IBInspectable var borderColor: UIColor? {
        get {
            return layer.borderColor.map(UIColor.init)
        }
        set {
            layer.borderColor = newValue?.cgColor
        }
    }
    
}


/// additional shadow customization, disabled for common views
@IBDesignable
class ShadowView: UIView {
    
    /// shadow color
    @IBInspectable var shadowColor: UIColor? {
        get {
            guard let shadowColor = layer.shadowColor else { return nil }
            return UIColor(cgColor: shadowColor)
        }
        set {
            layer.shadowColor = newValue?.cgColor
        }
    }
    
    /// shadow opacity
    @IBInspectable var shadowOpacity: Float {
        get {
            return layer.shadowOpacity
        }
        set {
            layer.shadowOpacity = newValue
        }
    }
    
    /// shadow offset
    @IBInspectable var shadowOffset: CGSize {
        get {
            return layer.shadowOffset
        }
        set {
            layer.shadowOffset = newValue
        }
    }
    
    /// shadow radius
    @IBInspectable var shadowRadius: CGFloat {
        get {
            return layer.shadowRadius
        }
        set {
            layer.shadowRadius = newValue
        }
    }
    
}

// MARK: - substrings
extension String {
    /// index starting from
    func index(from: Int) -> Index {
        return self.index(startIndex, offsetBy: from)
    }
    
    /// substring from index
    func substring(from: Int) -> String {
        let fromIndex = index(from: from)
        return String(self[fromIndex...])
    }
    
    /// substring to index
    func substring(to: Int) -> String {
        let toIndex = index(from: to)
        return String(self[..<toIndex])
    }
    
    /// substring with range
    func substring(with r: Range<Int>) -> String {
        let startIndex = index(from: r.lowerBound)
        let endIndex = index(from: r.upperBound)
        return String(self[startIndex..<endIndex])
    }
    
}


// MARK: - color extensions
public extension UIColor {
    
    /**
     Creates new color with RGBA values from 0-255 for RGB and a from 0-1
     
     - parameter r: the red color
     - parameter g: the green color
     - parameter b: the blue color
     - parameter a: the alpha color
     */
    public convenience init(r: CGFloat, g: CGFloat, b: CGFloat, a: CGFloat = 1) {
        self.init(red: r / 255, green: g / 255, blue: b / 255, alpha: a)
    }
    
    /**
     Get UIColor from hex string, e.g. "FF0000" -> red color
     
     - parameter hexString: the hex string
     - returns: the UIColor instance or nil
     */
    public class func from(hexString: String) -> UIColor? {
        if hexString.count == 6 {
            let redStr = hexString.substring(to: 2)
            let greenStr = hexString.substring(with: 2..<4)
            let blueStr = hexString.substring(from: 4)
            return UIColor(
                r: CGFloat(Int(redStr, radix: 16)!),
                g: CGFloat(Int(greenStr, radix: 16)!),
                b: CGFloat(Int(blueStr, radix: 16)!))
        }
        return nil
    }
    
}
