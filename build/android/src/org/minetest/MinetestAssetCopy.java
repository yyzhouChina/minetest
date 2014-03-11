package org.minetest;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.Vector;

import android.app.Activity;
import android.content.res.AssetFileDescriptor;

import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.Display;
import android.widget.ProgressBar;
import android.widget.TextView;

public class MinetestAssetCopy extends Activity {
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		setContentView(R.layout.assetcopy);
		
		m_ProgressBar = (ProgressBar) findViewById(R.id.progressBar1);
		m_Filename = (TextView) findViewById(R.id.textView1);
		
		Display display = getWindowManager().getDefaultDisplay();
		m_ProgressBar.getLayoutParams().width = (int) (display.getWidth() * 0.8);
		m_ProgressBar.invalidate();
		
		m_AssetCopy = new copyAssetTask();
		m_AssetCopy.execute();
	}
	
	ProgressBar m_ProgressBar;
	TextView m_Filename;
	
	copyAssetTask m_AssetCopy;
	
	private class copyAssetTask extends AsyncTask<String, Integer, String>{
		
		private void copyElement(String name, String path) {
			String baseDir = Environment.getExternalStorageDirectory().getAbsolutePath();
			String full_path;
			if (path != "") {
				full_path = path + "/" + name;
			}
			else {
				full_path = name;
			}
			//is a folder read asset list
			if (m_foldernames.contains(full_path)) {

				Log.w("MinetestAssetCopy","Opening folder: " + full_path);
				File current_folder = new File(baseDir + "/" + full_path);
				current_folder.mkdir();
				try {
					String[] current_assets = getAssets().list(full_path);
					for(int i=0; i < current_assets.length; i++) {
						copyElement(current_assets[i],full_path);
					}
				} catch (IOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
			}
			//is a file just copy
			else {
				boolean refresh = true;
				
				File testme = new File(baseDir + "/" + full_path);
				
				if (testme.exists()) {
					long asset_filesize = 0;
					try {
						AssetFileDescriptor fd = getAssets().openFd(full_path);
						asset_filesize = fd.getLength();
					} catch (IOException e) {
						refresh = false;
					}
					
					long sdcard_filesize = testme.length();
					
					if (asset_filesize == sdcard_filesize) {
						refresh = false;
					}
					
				}
				
				if (refresh) {
					m_tocopy.add(full_path);
				}
				else {
					Log.w("MinetestAssetCopy","Already present: " + full_path);
				}
			}
		}

		@Override
		protected String doInBackground(String... files) {
			
			m_foldernames  = new Vector<String>();
			m_tocopy       = new Vector<String>();
			String baseDir = Environment.getExternalStorageDirectory().getAbsolutePath() + "/";
			
			try {
				InputStream is = getAssets().open("index.txt");
				BufferedReader reader = new BufferedReader(new InputStreamReader(is));
		
				String line = reader.readLine();
				while(line != null){
					Log.w("MinetestAssetCopy",m_foldernames.size() + ": " + line);
					m_foldernames.add(line);
					line = reader.readLine();
				}
			} catch (IOException e1) {
				// TODO Auto-generated catch block
				e1.printStackTrace();
			}
			
			copyElement("Minetest","");
			
			Log.w("MinetestAssetCopy","Files to copy: " + m_tocopy.size());
			
			
			m_ProgressBar.setMax(m_tocopy.size());
			
			for (int i = 0; i < m_tocopy.size(); i++) {
				String filename = m_tocopy.get(i);
				Log.w("MinetestAssetCopy","Updating ui: " + filename);
				publishProgress(i);
				
				Log.w("MinetestAssetCopy","Copying file: " + m_tocopy.get(i));
				try {
					InputStream src = getAssets().open(filename);
					OutputStream dst = new FileOutputStream(baseDir + "/" + filename);

					// Transfer bytes from in to out
					byte[] buf = new byte[1024];
					int len;
					while ((len = src.read(buf)) > 0) {
						dst.write(buf, 0, len);
					}
					src.close();
					dst.close();
				} catch (IOException e) {
					Log.w("MinetestAssetCopy","Copying file: " + baseDir + "/" + filename + " FAILED");
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
				
			}
			return "";
		}
		
		protected void onProgressUpdate(Integer... progress) {
			m_ProgressBar.setProgress(progress[0]);
			m_Filename.setText(m_tocopy.get(progress[0]));
		}
		
		protected void onPostExecute (String result) {
			finish();
		}
		
		Vector<String> m_foldernames;
		Vector<String> m_tocopy;
	}
}
