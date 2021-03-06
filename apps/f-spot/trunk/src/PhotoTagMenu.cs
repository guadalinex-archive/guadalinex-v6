//
// PhotoTagMenu.cs
//
// Copyright (C) 2004 Novell, Inc.
//
//
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

using System;
using System.Collections;
using Gtk;

using FSpot;

public class PhotoTagMenu : Menu {
	public delegate void TagSelectedHandler (Tag t);
	public event TagSelectedHandler TagSelected;
	// This should be reworked to use a Selection interface to
	// extract the current selection
	private class TagMenuItem : Gtk.ImageMenuItem {
		public Tag Value;

		public TagMenuItem (Tag t) : base (t.Name) {
			Value = t;
			if (t.Icon != null)
				this.Image = new Gtk.Image (t.SizedIcon);
		}

		protected TagMenuItem (IntPtr raw) : base (raw) {}
	}

	public PhotoTagMenu () : base () {
	}
	
	protected PhotoTagMenu (IntPtr raw) : base (raw) {}

	public void Populate (Photo [] photos) {
		Hashtable hash = new Hashtable ();
		if (photos != null) {
			foreach (Photo p in photos) {
				foreach (Tag t in p.Tags) {
					if (!hash.Contains (t.Id)) {
						hash.Add (t.Id, t);
					}
				}
			}
		}

		foreach (Widget w in this.Children) {
			w.Destroy ();
		}
		
		if (hash.Count == 0) {
			/* Fixme this should really set parent menu
			   items insensitve */
			MenuItem item = new MenuItem (Mono.Unix.Catalog.GetString ("(No Tags)"));
			this.Append (item);
			item.Sensitive = false;
			item.ShowAll ();
			return;
		}

		foreach (Tag t in hash.Values) {
			TagMenuItem item = new TagMenuItem (t);
			this.Append (item);
			item.ShowAll ();
			item.Activated += HandleActivate;
		}
				
	}

	void HandleActivate (object obj, EventArgs args)
	{
		if (TagSelected != null) {
			TagMenuItem t = obj as TagMenuItem;
			if (t != null)
				TagSelected (t.Value);
			else 
				Console.WriteLine ("Item was not a TagMenuItem");
		}
	}
}
