import React, { useState, useEffect } from 'react';
import { StyleSheet, View, Text, TouchableOpacity, Modal } from 'react-native';
import { BarCodeScanner } from 'expo-barcode-scanner';
import QRCode from 'react-native-qrcode-svg';

const colors = {
    bg: '#0a0a0f',
    card: '#1a1a25',
    primary: '#6366f1',
    text: '#ffffff',
    textSecondary: '#a0a0b0',
    border: 'rgba(255,255,255,0.1)',
};

// QR Code Display Component
export function QRCodeDisplay({ value, size = 200 }) {
    return (
        <View style={styles.qrContainer}>
            <QRCode
                value={value}
                size={size}
                color={colors.text}
                backgroundColor={colors.card}
            />
            <Text style={styles.qrLabel}>Scan to receive QP</Text>
        </View>
    );
}

// QR Code Scanner Component
export function QRScanner({ onScan, onClose }) {
    const [hasPermission, setHasPermission] = useState(null);
    const [scanned, setScanned] = useState(false);

    useEffect(() => {
        (async () => {
            const { status } = await BarCodeScanner.requestPermissionsAsync();
            setHasPermission(status === 'granted');
        })();
    }, []);

    const handleBarCodeScanned = ({ data }) => {
        setScanned(true);
        onScan(data);
        onClose();
    };

    if (hasPermission === null) {
        return (
            <View style={styles.scannerContainer}>
                <Text style={styles.scannerText}>Requesting camera permission...</Text>
            </View>
        );
    }

    if (hasPermission === false) {
        return (
            <View style={styles.scannerContainer}>
                <Text style={styles.scannerText}>No access to camera</Text>
                <TouchableOpacity style={styles.closeButton} onPress={onClose}>
                    <Text style={styles.closeButtonText}>Close</Text>
                </TouchableOpacity>
            </View>
        );
    }

    return (
        <View style={styles.scannerContainer}>
            <BarCodeScanner
                onBarCodeScanned={scanned ? undefined : handleBarCodeScanned}
                style={StyleSheet.absoluteFillObject}
            />
            <View style={styles.scannerOverlay}>
                <View style={styles.scannerFrame} />
                <Text style={styles.scannerHint}>Point camera at QR code</Text>
            </View>
            <TouchableOpacity style={styles.closeButton} onPress={onClose}>
                <Text style={styles.closeButtonText}>✕ Cancel</Text>
            </TouchableOpacity>
        </View>
    );
}

// QR Modal for Wallet
export function QRModal({ visible, address, onClose }) {
    return (
        <Modal
            animationType="slide"
            transparent={true}
            visible={visible}
            onRequestClose={onClose}
        >
            <View style={styles.modalOverlay}>
                <View style={styles.modalContent}>
                    <Text style={styles.modalTitle}>Your Wallet QR</Text>
                    <QRCodeDisplay value={address || 'quantumpulse://empty'} size={200} />
                    <Text style={styles.addressText}>{address}</Text>
                    <TouchableOpacity style={styles.primaryButton} onPress={onClose}>
                        <Text style={styles.buttonText}>Close</Text>
                    </TouchableOpacity>
                </View>
            </View>
        </Modal>
    );
}

// Scanner Modal
export function ScannerModal({ visible, onScan, onClose }) {
    if (!visible) return null;

    return (
        <Modal
            animationType="slide"
            transparent={false}
            visible={visible}
            onRequestClose={onClose}
        >
            <QRScanner onScan={onScan} onClose={onClose} />
        </Modal>
    );
}

const styles = StyleSheet.create({
    qrContainer: {
        alignItems: 'center',
        padding: 20,
        backgroundColor: colors.card,
        borderRadius: 16,
        marginVertical: 16,
    },
    qrLabel: {
        marginTop: 16,
        fontSize: 14,
        color: colors.textSecondary,
    },
    scannerContainer: {
        flex: 1,
        backgroundColor: colors.bg,
        justifyContent: 'center',
        alignItems: 'center',
    },
    scannerText: {
        fontSize: 16,
        color: colors.text,
    },
    scannerOverlay: {
        ...StyleSheet.absoluteFillObject,
        justifyContent: 'center',
        alignItems: 'center',
    },
    scannerFrame: {
        width: 250,
        height: 250,
        borderWidth: 2,
        borderColor: colors.primary,
        borderRadius: 16,
        backgroundColor: 'transparent',
    },
    scannerHint: {
        marginTop: 20,
        fontSize: 16,
        color: colors.text,
    },
    closeButton: {
        position: 'absolute',
        bottom: 50,
        backgroundColor: colors.card,
        paddingHorizontal: 30,
        paddingVertical: 15,
        borderRadius: 12,
    },
    closeButtonText: {
        fontSize: 16,
        color: colors.text,
        fontWeight: '600',
    },
    modalOverlay: {
        flex: 1,
        backgroundColor: 'rgba(0,0,0,0.8)',
        justifyContent: 'center',
        alignItems: 'center',
    },
    modalContent: {
        backgroundColor: colors.card,
        borderRadius: 20,
        padding: 24,
        alignItems: 'center',
        width: '85%',
    },
    modalTitle: {
        fontSize: 20,
        fontWeight: '600',
        color: colors.text,
        marginBottom: 20,
    },
    addressText: {
        fontSize: 12,
        color: colors.textSecondary,
        marginTop: 16,
        textAlign: 'center',
        fontFamily: 'monospace',
    },
    primaryButton: {
        marginTop: 20,
        backgroundColor: colors.primary,
        paddingHorizontal: 40,
        paddingVertical: 14,
        borderRadius: 12,
    },
    buttonText: {
        fontSize: 16,
        color: colors.text,
        fontWeight: '600',
    },
});

export default { QRCodeDisplay, QRScanner, QRModal, ScannerModal };
