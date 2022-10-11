<?php

namespace App\Http\Controllers;

class ExampleController extends Controller
{
    public function home(\Illuminate\Http\Request $request)
    {
        return response()->json([
            "hep" => $request->all()
        ]);
    }
}
