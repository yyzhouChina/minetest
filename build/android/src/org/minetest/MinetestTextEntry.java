package org.minetest;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.widget.EditText;

public class MinetestTextEntry extends Activity {
	public AlertDialog mTextInputDialog;
	public EditText mTextInputWidget;
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		Bundle b = getIntent().getExtras();
		String caption      = b.getString("caption");
		String text         = b.getString("text");
		String acceptButton = b.getString("acceptButton");
		String cancelButton = b.getString("cancelButton");
		String hint         = b.getString("hint");
		
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		mTextInputWidget = new EditText(this);
		mTextInputWidget.setHint(hint);
		builder.setTitle(caption);
		builder.setMessage(text);
		builder.setNegativeButton(cancelButton, null);
		
		builder.setView(mTextInputWidget);
		builder.setPositiveButton(acceptButton, new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int whichButton) 
			{ pushResult(mTextInputWidget.getText().toString()); }
			});
		
		builder.setNegativeButton(cancelButton, new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int whichButton) {
				dialog.cancel();
				cancelDialog();
				}
			});
		
		mTextInputDialog = builder.create();
		Log.w("MinetestTextEntry", "Activity created caption=\"" + caption 
				+ "\" text=\"" + text + "\" acceptButton=\"" + acceptButton 
				+ "\" cancelButton=\"" + cancelButton + "\" hint=\"" + hint + "\"");
		mTextInputDialog.show();
	}
	
	public void pushResult(String text) {
		Intent resultData = new Intent();
		resultData.putExtra("text", text);
		setResult(Activity.RESULT_OK,resultData);
		finish();
	}
	
	public void cancelDialog() {
		setResult(Activity.RESULT_CANCELED);
		finish();
	}
}
