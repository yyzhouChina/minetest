package org.minetest;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.text.InputType;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnKeyListener;
import android.widget.EditText;

public class MinetestTextEntry extends Activity {
	public AlertDialog mTextInputDialog;
	public EditText mTextInputWidget;
	
	private final int MultiLineTextInput              = 1;
	private final int SingleLineTextInput             = 2;
	private final int SingleLinePasswordInput         = 3;
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		Bundle b = getIntent().getExtras();
		String caption      = b.getString("caption");
		String text         = b.getString("text");
		String acceptButton = b.getString("acceptButton");
		String cancelButton = b.getString("cancelButton");
		String hint         = b.getString("hint");
		String current      = b.getString("current");
		int    editType     = b.getInt("editType");
		
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		mTextInputWidget = new EditText(this);
		mTextInputWidget.setHint(hint);
		mTextInputWidget.setText(current);
		builder.setTitle(caption);
		builder.setMessage(text);
		builder.setNegativeButton(cancelButton, null);
		
		builder.setView(mTextInputWidget);
		
		if (editType == MultiLineTextInput) {
			builder.setPositiveButton(acceptButton, new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int whichButton) 
				{ pushResult(mTextInputWidget.getText().toString()); }
				});
		}
		
		builder.setNegativeButton(cancelButton, new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int whichButton) {
				dialog.cancel();
				cancelDialog();
				}
			});
		
		builder.setOnCancelListener(new DialogInterface.OnCancelListener() {
			public void onCancel(DialogInterface dialog) {
				cancelDialog();
			}
		});
		
		if ((editType == SingleLineTextInput) || 
				(editType == SingleLinePasswordInput)) {
			mTextInputWidget.setImeActionLabel(acceptButton,KeyEvent.KEYCODE_ENTER);
			mTextInputWidget.setOnKeyListener(new OnKeyListener() {
				@Override
				public boolean onKey(View view, int KeyCode, KeyEvent event) {
					if ( KeyCode == KeyEvent.KEYCODE_ENTER){
	
						pushResult(mTextInputWidget.getText().toString());
						return true;
					}
					// TODO Auto-generated method stub
					return false;
				}
			});
		}
		
		if (editType == SingleLinePasswordInput) {
			mTextInputWidget.setInputType(InputType.TYPE_CLASS_TEXT | 
					InputType.TYPE_TEXT_VARIATION_PASSWORD);
		}
		else {
			mTextInputWidget.setInputType(InputType.TYPE_CLASS_TEXT);
		}
		
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
