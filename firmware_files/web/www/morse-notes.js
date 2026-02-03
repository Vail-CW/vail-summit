// Morse Notes Web Interface
// JavaScript for managing and playing back Morse code recordings

let recordings = [];
let currentEditId = null;
let currentDeleteId = null;
let currentAudioPlayer = null;

// ===================================
// INITIALIZATION
// ===================================

document.addEventListener('DOMContentLoaded', () => {
    loadRecordings();
});

// ===================================
// API CALLS
// ===================================

async function loadRecordings() {
    try {
        const response = await fetch('/api/morse-notes/list');
        if (!response.ok) throw new Error('Failed to load recordings');

        const data = await response.json();
        recordings = data.recordings || [];

        displayRecordings();
    } catch (error) {
        console.error('Error loading recordings:', error);
        showToast('Failed to load recordings', true);
        document.getElementById('loadingMessage').textContent = 'Failed to load recordings';
    }
}

async function updateRecordingTitle(id, newTitle) {
    try {
        const response = await fetch(`/api/morse-notes/update?id=${id}&title=${encodeURIComponent(newTitle)}`, {
            method: 'PUT'
        });

        if (!response.ok) throw new Error('Failed to update title');

        const data = await response.json();
        if (data.success) {
            showToast('Title updated successfully');
            loadRecordings(); // Reload list
            return true;
        }
        throw new Error('Update failed');
    } catch (error) {
        console.error('Error updating title:', error);
        showToast('Failed to update title', true);
        return false;
    }
}

async function deleteRecording(id) {
    try {
        const response = await fetch(`/api/morse-notes/delete?id=${id}`, {
            method: 'DELETE'
        });

        if (!response.ok) throw new Error('Failed to delete recording');

        const data = await response.json();
        if (data.success) {
            showToast('Recording deleted successfully');
            loadRecordings(); // Reload list
            return true;
        }
        throw new Error('Delete failed');
    } catch (error) {
        console.error('Error deleting recording:', error);
        showToast('Failed to delete recording', true);
        return false;
    }
}

// ===================================
// DISPLAY FUNCTIONS
// ===================================

function displayRecordings() {
    const loadingEl = document.getElementById('loadingMessage');
    const listEl = document.getElementById('recordingList');
    const emptyEl = document.getElementById('emptyState');

    loadingEl.style.display = 'none';

    if (recordings.length === 0) {
        listEl.style.display = 'none';
        emptyEl.style.display = 'block';
        return;
    }

    emptyEl.style.display = 'none';
    listEl.style.display = 'grid';

    // Sort by timestamp (newest first)
    recordings.sort((a, b) => b.timestamp - a.timestamp);

    listEl.innerHTML = recordings.map(rec => createRecordingHTML(rec)).join('');
}

function createRecordingHTML(rec) {
    const date = new Date(rec.timestamp * 1000);
    const dateStr = date.toLocaleDateString() + ' ' + date.toLocaleTimeString();
    const durationStr = formatDuration(rec.durationMs);

    return `
        <div class="recording-item" data-id="${rec.id}">
            <div class="recording-info">
                <h3>${escapeHtml(rec.title)}</h3>
                <div class="recording-meta">
                    <div class="meta-item">
                        <span>📅</span>
                        <span>${dateStr}</span>
                    </div>
                    <div class="meta-item">
                        <span>⏱️</span>
                        <span>${durationStr}</span>
                    </div>
                    <div class="meta-item">
                        <span>📊</span>
                        <span>${rec.avgWPM.toFixed(1)} WPM</span>
                    </div>
                    <div class="meta-item">
                        <span>🎵</span>
                        <span>${rec.toneFrequency} Hz</span>
                    </div>
                    <div class="meta-item">
                        <span>📝</span>
                        <span>${rec.eventCount} events</span>
                    </div>
                </div>
                <div class="audio-player" id="player-${rec.id}">
                    <audio controls preload="none">
                        <source src="/api/morse-notes/export/wav?id=${rec.id}" type="audio/wav">
                        Your browser does not support audio playback.
                    </audio>
                </div>
            </div>
            <div class="recording-actions">
                <button class="btn btn-play btn-small" onclick="togglePlay(${rec.id})">
                    ▶️ Play
                </button>
                <button class="btn btn-wav btn-small" onclick="downloadWAV(${rec.id}, '${escapeHtml(rec.title)}')">
                    💾 WAV
                </button>
                <button class="btn btn-small" onclick="downloadMR(${rec.id}, '${escapeHtml(rec.title)}')">
                    📥 .mr
                </button>
                <button class="btn btn-edit btn-small" onclick="openEditModal(${rec.id}, '${escapeHtml(rec.title)}')">
                    ✏️ Edit
                </button>
                <button class="btn btn-delete btn-small" onclick="openDeleteModal(${rec.id}, '${escapeHtml(rec.title)}')">
                    🗑️ Delete
                </button>
            </div>
        </div>
    `;
}

// ===================================
// PLAYBACK FUNCTIONS
// ===================================

function togglePlay(id) {
    const playerEl = document.getElementById(`player-${id}`);
    const audioEl = playerEl.querySelector('audio');
    const btnEl = event.target;

    // Stop any other playing audio
    if (currentAudioPlayer && currentAudioPlayer !== audioEl) {
        currentAudioPlayer.pause();
        currentAudioPlayer.currentTime = 0;
        currentAudioPlayer.parentElement.classList.remove('active');
        // Reset other button text
        document.querySelectorAll('.btn-play').forEach(btn => {
            if (btn !== btnEl) {
                btn.textContent = '▶️ Play';
            }
        });
    }

    // Toggle current player
    if (playerEl.classList.contains('active')) {
        audioEl.pause();
        playerEl.classList.remove('active');
        btnEl.textContent = '▶️ Play';
        currentAudioPlayer = null;
    } else {
        playerEl.classList.add('active');
        audioEl.play().catch(err => {
            console.error('Playback error:', err);
            showToast('Failed to play audio', true);
        });
        btnEl.textContent = '⏸️ Pause';
        currentAudioPlayer = audioEl;

        // Reset button text when audio ends
        audioEl.addEventListener('ended', () => {
            btnEl.textContent = '▶️ Play';
            playerEl.classList.remove('active');
            currentAudioPlayer = null;
        });
    }
}

// ===================================
// DOWNLOAD FUNCTIONS
// ===================================

function downloadWAV(id, title) {
    const url = `/api/morse-notes/export/wav?id=${id}`;
    const filename = sanitizeFilename(title) + '.wav';
    downloadFile(url, filename);
    showToast('Downloading WAV file...');
}

function downloadMR(id, title) {
    const url = `/api/morse-notes/download?id=${id}`;
    const filename = sanitizeFilename(title) + '.mr';
    downloadFile(url, filename);
    showToast('Downloading .mr file...');
}

function downloadFile(url, filename) {
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
}

// ===================================
// EDIT MODAL
// ===================================

function openEditModal(id, currentTitle) {
    currentEditId = id;
    document.getElementById('editTitleInput').value = currentTitle;
    document.getElementById('editModal').classList.add('active');
    document.getElementById('editTitleInput').focus();

    // Allow Enter key to save
    document.getElementById('editTitleInput').onkeypress = (e) => {
        if (e.key === 'Enter') saveEdit();
    };
}

function cancelEdit() {
    currentEditId = null;
    document.getElementById('editModal').classList.remove('active');
}

async function saveEdit() {
    const newTitle = document.getElementById('editTitleInput').value.trim();

    if (!newTitle) {
        showToast('Title cannot be empty', true);
        return;
    }

    if (newTitle.length > 60) {
        showToast('Title too long (max 60 characters)', true);
        return;
    }

    const success = await updateRecordingTitle(currentEditId, newTitle);

    if (success) {
        cancelEdit();
    }
}

// ===================================
// DELETE MODAL
// ===================================

function openDeleteModal(id, title) {
    currentDeleteId = id;
    document.getElementById('deleteRecordingTitle').textContent = title;
    document.getElementById('deleteModal').classList.add('active');
}

function cancelDelete() {
    currentDeleteId = null;
    document.getElementById('deleteModal').classList.remove('active');
}

async function confirmDelete() {
    const success = await deleteRecording(currentDeleteId);

    if (success) {
        cancelDelete();
    }
}

// ===================================
// TOAST NOTIFICATIONS
// ===================================

function showToast(message, isError = false) {
    const toastEl = document.getElementById('toast');
    toastEl.textContent = message;

    if (isError) {
        toastEl.classList.add('error');
    } else {
        toastEl.classList.remove('error');
    }

    toastEl.classList.add('active');

    setTimeout(() => {
        toastEl.classList.remove('active');
    }, 3000);
}

// ===================================
// UTILITY FUNCTIONS
// ===================================

function formatDuration(ms) {
    const totalSeconds = Math.floor(ms / 1000);
    const minutes = Math.floor(totalSeconds / 60);
    const seconds = totalSeconds % 60;
    return `${minutes}:${seconds.toString().padStart(2, '0')}`;
}

function escapeHtml(text) {
    const map = {
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        '"': '&quot;',
        "'": '&#039;'
    };
    return text.replace(/[&<>"']/g, m => map[m]);
}

function sanitizeFilename(filename) {
    return filename
        .replace(/[^a-z0-9_\-]/gi, '_')
        .replace(/_+/g, '_')
        .replace(/^_|_$/g, '');
}

// Close modals when clicking outside
window.onclick = (event) => {
    if (event.target.classList.contains('modal')) {
        cancelEdit();
        cancelDelete();
    }
};

// Allow Escape key to close modals
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
        cancelEdit();
        cancelDelete();
    }
});
