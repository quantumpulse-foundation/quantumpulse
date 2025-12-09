import { useState, useEffect } from 'react'
import {
    Chart as ChartJS,
    CategoryScale,
    LinearScale,
    PointElement,
    LineElement,
    BarElement,
    ArcElement,
    Title,
    Tooltip,
    Legend,
    Filler
} from 'chart.js'
import { Line, Bar, Doughnut } from 'react-chartjs-2'

// Register Chart.js components
ChartJS.register(
    CategoryScale,
    LinearScale,
    PointElement,
    LineElement,
    BarElement,
    ArcElement,
    Title,
    Tooltip,
    Legend,
    Filler
)

// API Helper
const api = {
    async get(endpoint) {
        try {
            const res = await fetch(`/api${endpoint}`)
            return await res.json()
        } catch (err) {
            console.error('API Error:', err)
            return null
        }
    },
    async post(endpoint, data) {
        try {
            const res = await fetch(`/api${endpoint}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            })
            return await res.json()
        } catch (err) {
            console.error('API Error:', err)
            return null
        }
    }
}

// Chart theme
const chartTheme = {
    backgroundColor: 'rgba(99, 102, 241, 0.1)',
    borderColor: 'rgb(99, 102, 241)',
    pointBackgroundColor: 'rgb(139, 92, 246)',
    gridColor: 'rgba(255, 255, 255, 0.05)',
    textColor: '#a0a0b0'
}

// Price Chart Component
function PriceChart() {
    const labels = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec']

    const data = {
        labels,
        datasets: [{
            label: 'QP Price (USD)',
            data: [600000, 605000, 610000, 615000, 620000, 625000, 630000, 640000, 650000, 660000, 680000, 700000],
            fill: true,
            backgroundColor: 'rgba(99, 102, 241, 0.15)',
            borderColor: 'rgb(99, 102, 241)',
            tension: 0.4,
            pointRadius: 4,
            pointHoverRadius: 6
        }]
    }

    const options = {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
            legend: { display: false },
            tooltip: {
                callbacks: {
                    label: (ctx) => `$${ctx.raw.toLocaleString()}`
                }
            }
        },
        scales: {
            x: {
                grid: { color: chartTheme.gridColor },
                ticks: { color: chartTheme.textColor }
            },
            y: {
                grid: { color: chartTheme.gridColor },
                ticks: {
                    color: chartTheme.textColor,
                    callback: (v) => `$${(v / 1000)}K`
                },
                min: 590000
            }
        }
    }

    return (
        <div style={{ height: '300px' }}>
            <Line data={data} options={options} />
        </div>
    )
}

// Mining Chart Component
function MiningChart() {
    const data = {
        labels: ['Premined', 'Mined', 'Remaining'],
        datasets: [{
            data: [2000000, 0, 3000000],
            backgroundColor: [
                'rgba(139, 92, 246, 0.8)',
                'rgba(16, 185, 129, 0.8)',
                'rgba(55, 65, 81, 0.8)'
            ],
            borderWidth: 0
        }]
    }

    const options = {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
            legend: {
                position: 'bottom',
                labels: { color: chartTheme.textColor }
            },
            tooltip: {
                callbacks: {
                    label: (ctx) => `${ctx.label}: ${ctx.raw.toLocaleString()} QP`
                }
            }
        }
    }

    return (
        <div style={{ height: '250px' }}>
            <Doughnut data={data} options={options} />
        </div>
    )
}

// Hashrate Chart Component
function HashrateChart() {
    const labels = ['00:00', '04:00', '08:00', '12:00', '16:00', '20:00', 'Now']

    const data = {
        labels,
        datasets: [{
            label: 'Network Hashrate (MH/s)',
            data: [120, 135, 150, 180, 165, 190, 210],
            backgroundColor: 'rgba(16, 185, 129, 0.7)',
            borderRadius: 6
        }]
    }

    const options = {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
            legend: { display: false }
        },
        scales: {
            x: {
                grid: { display: false },
                ticks: { color: chartTheme.textColor }
            },
            y: {
                grid: { color: chartTheme.gridColor },
                ticks: { color: chartTheme.textColor }
            }
        }
    }

    return (
        <div style={{ height: '200px' }}>
            <Bar data={data} options={options} />
        </div>
    )
}

// Dashboard Page with Charts
function Dashboard({ info, price }) {
    return (
        <>
            <div className="header">
                <h2>Dashboard</h2>
                <div className="header-right">
                    <div className="status-badge">
                        <span className="status-dot"></span>
                        Network Active
                    </div>
                </div>
            </div>

            <div className="price-display">
                <div className="price-label">Current QP Price</div>
                <div className="price-value">${(price?.price || 600000).toLocaleString()}</div>
                <div className="price-guarantee">
                    ✓ Minimum $600,000 Guaranteed
                </div>
            </div>

            <div className="stats-grid">
                <div className="stat-card">
                    <div className="stat-header">
                        <span className="stat-icon">⛓️</span>
                    </div>
                    <div className="stat-value">{info?.chainLength || 16}</div>
                    <div className="stat-label">Total Blocks</div>
                </div>

                <div className="stat-card">
                    <div className="stat-header">
                        <span className="stat-icon">💰</span>
                    </div>
                    <div className="stat-value">{(info?.totalMinedCoins || 0).toLocaleString()}</div>
                    <div className="stat-label">Mined Coins (QP)</div>
                </div>

                <div className="stat-card">
                    <div className="stat-header">
                        <span className="stat-icon">🏦</span>
                    </div>
                    <div className="stat-value">2,000,000</div>
                    <div className="stat-label">Premined</div>
                </div>

                <div className="stat-card">
                    <div className="stat-header">
                        <span className="stat-icon">🔗</span>
                    </div>
                    <div className="stat-value">{info?.shards || 2048}</div>
                    <div className="stat-label">Shards</div>
                </div>
            </div>

            <div className="stats-grid" style={{ gridTemplateColumns: '2fr 1fr' }}>
                <div className="card">
                    <div className="card-header">
                        <span className="card-title">📈 Price History</span>
                    </div>
                    <PriceChart />
                </div>

                <div className="card">
                    <div className="card-header">
                        <span className="card-title">💎 Coin Distribution</span>
                    </div>
                    <MiningChart />
                </div>
            </div>

            <div className="card">
                <div className="card-header">
                    <span className="card-title">⚡ Network Hashrate (24h)</span>
                </div>
                <HashrateChart />
            </div>
        </>
    )
}

// Wallet Page
function WalletPage() {
    const [walletName, setWalletName] = useState('')
    const [password, setPassword] = useState('')
    const [address, setAddress] = useState('')
    const [balance, setBalance] = useState(0)
    const [hasWallet, setHasWallet] = useState(false)

    const createWallet = () => {
        if (walletName && password) {
            const addr = 'pub_v11_' + Math.random().toString(36).substring(2, 15)
            setAddress(addr)
            setHasWallet(true)
            setBalance(0)
        }
    }

    return (
        <>
            <div className="header">
                <h2>Wallet</h2>
            </div>

            {!hasWallet ? (
                <div className="card">
                    <div className="card-header">
                        <span className="card-title">Create New Wallet</span>
                    </div>
                    <div className="input-group">
                        <label>Wallet Name</label>
                        <input
                            type="text"
                            placeholder="My Wallet"
                            value={walletName}
                            onChange={(e) => setWalletName(e.target.value)}
                        />
                    </div>
                    <div className="input-group">
                        <label>Password</label>
                        <input
                            type="password"
                            placeholder="Enter strong password"
                            value={password}
                            onChange={(e) => setPassword(e.target.value)}
                        />
                    </div>
                    <button className="btn btn-primary" onClick={createWallet}>
                        Create Wallet
                    </button>
                </div>
            ) : (
                <>
                    <div className="wallet-balance">
                        <div className="price-label">Your Balance</div>
                        <div className="price-value">{balance.toLocaleString()} QP</div>
                        <div className="price-guarantee">≈ ${(balance * 600000).toLocaleString()} USD</div>
                    </div>

                    <div className="card">
                        <div className="card-header">
                            <span className="card-title">Your Address</span>
                            <button className="btn btn-secondary" onClick={() => navigator.clipboard.writeText(address)}>
                                📋 Copy
                            </button>
                        </div>
                        <div className="wallet-address">{address}</div>
                    </div>

                    <div className="card">
                        <div className="card-header">
                            <span className="card-title">Send QP</span>
                        </div>
                        <div className="input-group">
                            <label>Recipient Address</label>
                            <input type="text" placeholder="pub_v11_..." />
                        </div>
                        <div className="input-group">
                            <label>Amount (QP)</label>
                            <input type="number" placeholder="0.00" />
                        </div>
                        <button className="btn btn-primary">💸 Send Transaction</button>
                    </div>
                </>
            )}
        </>
    )
}

// Mining Page with Chart
function MiningPage({ info }) {
    const [isMining, setIsMining] = useState(false)
    const [hashrate, setHashrate] = useState(0)

    const toggleMining = async () => {
        if (!isMining) {
            await api.post('/mine', {})
            setIsMining(true)
            setHashrate(Math.floor(Math.random() * 100) + 50)
        } else {
            setIsMining(false)
            setHashrate(0)
        }
    }

    return (
        <>
            <div className="header">
                <h2>Mining</h2>
            </div>

            <div className="stats-grid">
                <div className="stat-card">
                    <div className="stat-header">
                        <span className="stat-icon">⚡</span>
                    </div>
                    <div className="stat-value">{hashrate} MH/s</div>
                    <div className="stat-label">Your Hashrate</div>
                </div>

                <div className="stat-card">
                    <div className="stat-header">
                        <span className="stat-icon">🎯</span>
                    </div>
                    <div className="stat-value">4</div>
                    <div className="stat-label">Difficulty</div>
                </div>

                <div className="stat-card">
                    <div className="stat-header">
                        <span className="stat-icon">🏆</span>
                    </div>
                    <div className="stat-value">50 QP</div>
                    <div className="stat-label">Block Reward</div>
                </div>
            </div>

            <div className="card">
                <div className="card-header">
                    <span className="card-title">Mining Controls</span>
                </div>
                <div className="mining-controls">
                    <button
                        className={`btn ${isMining ? 'btn-secondary' : 'btn-primary'}`}
                        onClick={toggleMining}
                    >
                        {isMining ? '⏹️ Stop Mining' : '▶️ Start Mining'}
                    </button>
                </div>

                <div className="mining-stat">
                    <span>Status</span>
                    <span style={{ color: isMining ? 'var(--success)' : 'var(--text-secondary)' }}>
                        {isMining ? '● Mining Active' : '○ Idle'}
                    </span>
                </div>
                <div className="mining-stat">
                    <span>Mining Limit</span>
                    <span>3,000,000 QP</span>
                </div>
                <div className="mining-stat">
                    <span>Mined So Far</span>
                    <span>{info?.totalMinedCoins || 0} QP</span>
                </div>
            </div>

            <div className="card">
                <div className="card-header">
                    <span className="card-title">📊 Mining Progress</span>
                </div>
                <MiningChart />
            </div>
        </>
    )
}

// Transactions Page
function TransactionsPage() {
    const transactions = [
        { id: 'tx_001...', from: 'pub_v11_abc...', to: 'pub_v11_xyz...', amount: 100, status: 'confirmed', time: '2 min ago' },
        { id: 'tx_002...', from: 'Shankar-Lal-Khati', to: 'pub_v11_def...', amount: 500, status: 'confirmed', time: '5 min ago' },
        { id: 'tx_003...', from: 'pub_v11_ghi...', to: 'pub_v11_jkl...', amount: 25, status: 'pending', time: 'Just now' },
    ]

    return (
        <>
            <div className="header">
                <h2>Transactions</h2>
            </div>

            <div className="card">
                <div className="card-header">
                    <span className="card-title">Recent Transactions</span>
                    <button className="btn btn-secondary">🔄 Refresh</button>
                </div>
                <div className="table-container">
                    <table>
                        <thead>
                            <tr>
                                <th>TX ID</th>
                                <th>From</th>
                                <th>To</th>
                                <th>Amount</th>
                                <th>Status</th>
                                <th>Time</th>
                            </tr>
                        </thead>
                        <tbody>
                            {transactions.map((tx, i) => (
                                <tr key={i}>
                                    <td style={{ fontFamily: 'monospace' }}>{tx.id}</td>
                                    <td>{tx.from}</td>
                                    <td>{tx.to}</td>
                                    <td>{tx.amount} QP</td>
                                    <td>
                                        <span style={{
                                            padding: '4px 12px',
                                            borderRadius: '12px',
                                            background: tx.status === 'confirmed' ? 'rgba(16, 185, 129, 0.2)' : 'rgba(245, 158, 11, 0.2)',
                                            color: tx.status === 'confirmed' ? 'var(--success)' : 'var(--warning)'
                                        }}>
                                            {tx.status}
                                        </span>
                                    </td>
                                    <td style={{ color: 'var(--text-secondary)' }}>{tx.time}</td>
                                </tr>
                            ))}
                        </tbody>
                    </table>
                </div>
            </div>
        </>
    )
}

// Main App
export default function App() {
    const [page, setPage] = useState('dashboard')
    const [info, setInfo] = useState(null)
    const [price, setPrice] = useState(null)

    useEffect(() => {
        const fetchData = async () => {
            const infoData = await api.get('/info')
            const priceData = await api.get('/price')
            if (infoData) setInfo(infoData)
            if (priceData) setPrice(priceData)
        }

        fetchData()

        setInfo({
            version: '7.0.0',
            chainLength: 16,
            totalMinedCoins: 0,
            shards: 2048
        })
        setPrice({ price: 600000, guaranteed: true })
    }, [])

    const renderPage = () => {
        switch (page) {
            case 'dashboard': return <Dashboard info={info} price={price} />
            case 'wallet': return <WalletPage />
            case 'mining': return <MiningPage info={info} />
            case 'transactions': return <TransactionsPage />
            default: return <Dashboard info={info} price={price} />
        }
    }

    return (
        <div className="app">
            <aside className="sidebar">
                <div className="logo">
                    <div className="logo-icon">Q</div>
                    <div className="logo-text">
                        <h1>QuantumPulse</h1>
                        <span>v7.0.0</span>
                    </div>
                </div>

                <nav className="nav">
                    <button
                        className={`nav-item ${page === 'dashboard' ? 'active' : ''}`}
                        onClick={() => setPage('dashboard')}
                    >
                        <span className="nav-icon">📊</span>
                        Dashboard
                    </button>
                    <button
                        className={`nav-item ${page === 'wallet' ? 'active' : ''}`}
                        onClick={() => setPage('wallet')}
                    >
                        <span className="nav-icon">👛</span>
                        Wallet
                    </button>
                    <button
                        className={`nav-item ${page === 'mining' ? 'active' : ''}`}
                        onClick={() => setPage('mining')}
                    >
                        <span className="nav-icon">⛏️</span>
                        Mining
                    </button>
                    <button
                        className={`nav-item ${page === 'transactions' ? 'active' : ''}`}
                        onClick={() => setPage('transactions')}
                    >
                        <span className="nav-icon">💸</span>
                        Transactions
                    </button>
                </nav>

                <div style={{ marginTop: 'auto', padding: '16px', borderTop: '1px solid var(--border)' }}>
                    <a href="/docs/" target="_blank" className="nav-item" style={{ textDecoration: 'none' }}>
                        <span className="nav-icon">📚</span>
                        API Docs
                    </a>
                </div>
            </aside>

            <main className="main-content">
                {renderPage()}
            </main>
        </div>
    )
}
