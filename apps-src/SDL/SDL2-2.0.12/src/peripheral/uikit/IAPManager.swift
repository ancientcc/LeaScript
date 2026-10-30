// IAPManager.swift
import StoreKit

@MainActor
@objc public class IAPManager: NSObject {
    @objc public static let shared = IAPManager()
    
    // Purchase callback, passed in from C++ via a function pointer.
    private var purchaseCallback: ((Int32, String?, Int32, String?) -> Void)?
    
    @objc public func setPurchaseCallback(_ callback: @escaping (Int32, String?, Int32) -> Void) {
        self.purchaseCallback = callback
        // Listen for transaction updates (handle unfinished transactions)
        startTransactionListener()
    }
    
    @objc(purchaseWithProductID:transactionId:) public func purchase(productID: String, transactionId: Int32) {
        Task {
            do {
                let products = try await Product.products(for: [productID])
                guard let product = products.first else {
                    self.purchaseCallback?(-1, "Product not found", transactionId, nil)
                    return
                }
                
                let result = try await product.purchase()
                switch result {
                case .success(let verification):
                    switch verification {
                    case .verified(let transaction):
                        await transaction.finish()
                        // Verification successful, complete the transaction.
                        self.purchaseCallback?(0, transaction.productID, transactionId, transaction.jwsRepresentation)
                    case .unverified(_, let error):
                        self.purchaseCallback?(-2, error.localizedDescription, transactionId, nil)
                    }
                case .pending:
                    self.purchaseCallback?(-3, "Purchase pending", transactionId, nil)
                case .userCancelled:
                    self.purchaseCallback?(-4, "User cancelled", transactionId, nil)
                @unknown default:
                    self.purchaseCallback?(-5, "Unknown result", transactionId, nil)
                }
            } catch {
                self.purchaseCallback?(-6, error.localizedDescription, transactionId, nil)
            }
        }
    }
    
    @objc public func restorePurchases() {
        Task {
            try? await AppStore.sync()
            // Check current entitlements.
            for await result in Transaction.currentEntitlements {
                if case .verified(let transaction) = result {
                    self.purchaseCallback?(0, transaction.productID, 0, nil)
                }
            }
            // Loop ends, notify the C++ layer that "restoration is complete"
            self.purchaseCallback?(1, nil, 0, nil)
        }
    }
    
    private func startTransactionListener() {
        Task(priority: .background) {
            for await result in Transaction.updates {
                if case .verified(let transaction) = result {
                    await transaction.finish()
                }
            }
        }
    }
}
