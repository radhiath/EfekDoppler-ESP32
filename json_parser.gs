function doPost(e) {
  // Parsing data JSON dari request
  let data;
  try {
    data = JSON.parse(e.postData.contents);
  } catch (err) {
    return ContentService.createTextOutput("Format JSON salah");
  }

  // Validasi input
  if (!data.mode || data.temp == null || data.src == null || data.obsv == null) {
    return ContentService.createTextOutput("Data tidak lengkap");
  }

  // Konfigurasi mode dan sheet berdasarkan mode
  const modeConfig = {
    "0": { sheetName: "0. Debug Only", sourceVelocity: 0, observerVelocity: 0},
    "1": { sheetName: "1. Sumber dan Pendengar Diam", sourceVelocity: 0, observerVelocity: 0 },
    "2": { sheetName: "2. Sumber Mendekati Pendengar Diam", sourceVelocity: -0.089, observerVelocity: 0 },
    "3": { sheetName: "3. Sumber Menjauhi Pendengar Diam", sourceVelocity: +0.089, observerVelocity: 0 },
    "4": { sheetName: "4. Pendengar Mendekati Sumber Diam", sourceVelocity: 0, observerVelocity: +0.093 },
    "5": { sheetName: "5. Pendengar Menjauhi Sumber Diam", sourceVelocity: 0, observerVelocity: -0.093 },
    "6": { sheetName: "6. Sumber dan Pendengar Saling Mendekati", sourceVelocity: -0.089, observerVelocity: +0.093 },
    "7": { sheetName: "7. Sumber dan Pendengar Saling Menjauhi", sourceVelocity: +0.089, observerVelocity: -0.093 }
  };

  // Ambil konfigurasi berdasarkan mode
  const { sheetName, sourceVelocity, observerVelocity } = modeConfig[data.mode] || {};

  // if (!sheetName) {
  //   return ContentService.createTextOutput("Nama seeet salah");
  // }

  // Akses sheet berdasarkan nama
  let sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName(sheetName);

  // if (!sheet) {
  //   sheet = SpreadsheetApp.getActiveSpreadsheet().insertSheet(sheetName);
  // }

  // Header kolom
  const header = [
    "Suhu (T)", "Kecepatan Suara (v)",
    "Frekuensi Sumber (f_s)", "Kecepatan Sumber (v_s)",
    "Kecepatan Pendengar (v_p)", "Frekuensi Terdengar (f_p)",
    "Frekuensi Terdengar Ideal (f_p Ideal)", "Error",
    "Error Relatif"
  ];

  // Tambahin header kalo rest = true
  if (data.reset === true || data.reset === "true") {
    sheet.clear();
    sheet.appendRow(header);
  }

  // Ambil nilai dari data
  let temperature = Number(data.temp);
  let sourceFrequency = Number(data.src);
  let observedFrequency = Number(data.obsv);

  // Perhitungan buat data
  let speedOfSound = calculateSpeedOfSound(temperature);
  let idealFrequency = calculateIdealFrequency(speedOfSound, sourceFrequency, sourceVelocity, observerVelocity);
  let error = calculateError(observedFrequency, idealFrequency);
  let relativeError = calculateRelativeError(observedFrequency, idealFrequency);

  // Tambahin baris baru
  let dataRow = [
    temperature, speedOfSound,
    sourceFrequency, Math.abs(sourceVelocity),
    Math.abs(observerVelocity), observedFrequency,
    idealFrequency, error,
    relativeError
  ];
  sheet.appendRow(dataRow);

  return ContentService.createTextOutput("Data berhasil diproses");
}

// Helper functions
/**
 * Menghitung kecepatan suara berdasarkan suhu udara
 * 
 * Rumus: v = 331.3 + (0.606 * T)
 *
 * @param {number} temperature - Suhu udara (°C)
 * @returns {number} Kecepatan suara (m/s)
 */
function calculateSpeedOfSound(temperature) {
  return 331.3 + (0.606 * temperature);
}

/**
 * Menghitung frekuensi ideal yang diterima oleh pengamat
 * berdasarkan efek Doppler
 * 
 * Rumus: f' = f * ((v + v_observer) / (v + v_source))
 *
 * @param {number} speedOfSound - Kecepatan suara (m/s)
 * @param {number} sourceFrequency - Frekuensi sumber (Hz)
 * @param {number} sourceVelocity - Kecepatan sumber (m/s)
 * @param {number} observerVelocity - Kecepatan pengamat (m/s)
 * @returns {number} Frekuensi ideal yang diterima oleh pengamat
 */
function calculateIdealFrequency(speedOfSound, sourceFrequency, sourceVelocity, observerVelocity) {
  return sourceFrequency * ((speedOfSound + observerVelocity) / (speedOfSound + sourceVelocity));
}

/**
 * Menghitung error absolut antara frekuensi pengamatan
 * dan frekuensi ideal
 * 
 * Rumus: Error = f_observed - f_ideal
 *
 * @param {number} observedFrequency - Frekuensi yang diterima oleh pengamat (Hz)
 * @param {number} idealFrequency - Frekuensi ideal (Hz)
 * @returns {number} Selisih antara frekuensi pengamatan dan frekuensi ideal
 */
function calculateError(observedFrequency, idealFrequency) {
  return observedFrequency - idealFrequency;
}

/**
 * Menghitung error relatif dalam format persen
 * 
 * Rumus: Relative Error (%) = ((f_observed - f_ideal) / f_ideal) * 100
 *
 * @param {number} observedFrequency - Frekuensi yang diterima oleh pengamat (Hz)
 * @param {number} idealFrequency - Frekuensi ideal (Hz)
 * @returns {string} Error relatif (%)
 */
function calculateRelativeError(observedFrequency, idealFrequency) {
  return (((observedFrequency - idealFrequency) / idealFrequency) * 100).toFixed(5) + "%";
}