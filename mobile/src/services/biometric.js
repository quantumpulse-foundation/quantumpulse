import * as LocalAuthentication from 'expo-local-authentication';
import * as SecureStore from 'expo-secure-store';
import { Alert } from 'react-native';

// Biometric types
export const BiometricType = {
    FINGERPRINT: 'fingerprint',
    FACE_ID: 'faceId',
    IRIS: 'iris',
    NONE: 'none',
};

// Check if biometrics are available
export async function isBiometricAvailable() {
    const compatible = await LocalAuthentication.hasHardwareAsync();
    const enrolled = await LocalAuthentication.isEnrolledAsync();
    return compatible && enrolled;
}

// Get available biometric types
export async function getBiometricType() {
    const types = await LocalAuthentication.supportedAuthenticationTypesAsync();

    if (types.includes(LocalAuthentication.AuthenticationType.FACIAL_RECOGNITION)) {
        return BiometricType.FACE_ID;
    }
    if (types.includes(LocalAuthentication.AuthenticationType.FINGERPRINT)) {
        return BiometricType.FINGERPRINT;
    }
    if (types.includes(LocalAuthentication.AuthenticationType.IRIS)) {
        return BiometricType.IRIS;
    }
    return BiometricType.NONE;
}

// Authenticate with biometrics
export async function authenticateWithBiometrics(reason = 'Authenticate to access your wallet') {
    try {
        const available = await isBiometricAvailable();
        if (!available) {
            return { success: false, error: 'Biometrics not available' };
        }

        const result = await LocalAuthentication.authenticateAsync({
            promptMessage: reason,
            cancelLabel: 'Cancel',
            fallbackLabel: 'Use Passcode',
            disableDeviceFallback: false,
        });

        return {
            success: result.success,
            error: result.error
        };
    } catch (error) {
        return { success: false, error: error.message };
    }
}

// Enable biometric lock for wallet
export async function enableBiometricLock(walletId) {
    try {
        const auth = await authenticateWithBiometrics('Confirm your identity to enable biometric lock');
        if (!auth.success) {
            return false;
        }

        await SecureStore.setItemAsync(`biometric_enabled_${walletId}`, 'true');
        return true;
    } catch (error) {
        console.error('Error enabling biometric lock:', error);
        return false;
    }
}

// Disable biometric lock
export async function disableBiometricLock(walletId) {
    try {
        const auth = await authenticateWithBiometrics('Confirm your identity to disable biometric lock');
        if (!auth.success) {
            return false;
        }

        await SecureStore.deleteItemAsync(`biometric_enabled_${walletId}`);
        return true;
    } catch (error) {
        console.error('Error disabling biometric lock:', error);
        return false;
    }
}

// Check if biometric lock is enabled
export async function isBiometricLockEnabled(walletId) {
    try {
        const value = await SecureStore.getItemAsync(`biometric_enabled_${walletId}`);
        return value === 'true';
    } catch (error) {
        return false;
    }
}

// Authenticate for transaction
export async function authenticateForTransaction(amount) {
    const reason = `Authenticate to send ${amount} QP`;
    return await authenticateWithBiometrics(reason);
}

// Authenticate to view private key
export async function authenticateForPrivateKey() {
    return await authenticateWithBiometrics('Authenticate to view your private key');
}

// Authenticate to export wallet
export async function authenticateForExport() {
    return await authenticateWithBiometrics('Authenticate to export your wallet');
}

// Store sensitive data securely (requires biometric to retrieve)
export async function storeSecureData(key, value) {
    try {
        await SecureStore.setItemAsync(key, value, {
            requireAuthentication: true,
            authenticationPrompt: 'Authenticate to save secure data',
        });
        return true;
    } catch (error) {
        console.error('Error storing secure data:', error);
        return false;
    }
}

// Retrieve sensitive data (requires biometric)
export async function getSecureData(key) {
    try {
        return await SecureStore.getItemAsync(key, {
            requireAuthentication: true,
            authenticationPrompt: 'Authenticate to access secure data',
        });
    } catch (error) {
        console.error('Error retrieving secure data:', error);
        return null;
    }
}

// Biometric prompt component helper
export function showBiometricPrompt(onSuccess, onError, reason) {
    authenticateWithBiometrics(reason).then(result => {
        if (result.success) {
            onSuccess();
        } else {
            onError(result.error || 'Authentication failed');
        }
    });
}

export default {
    BiometricType,
    isBiometricAvailable,
    getBiometricType,
    authenticateWithBiometrics,
    enableBiometricLock,
    disableBiometricLock,
    isBiometricLockEnabled,
    authenticateForTransaction,
    authenticateForPrivateKey,
    authenticateForExport,
    storeSecureData,
    getSecureData,
    showBiometricPrompt,
};
